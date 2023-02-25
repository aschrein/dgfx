
// MIT License
//
// Copyright (c) 2023 Anton Schreiner
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#if !defined(EMBREE_HPP)
#    define EMBREE_HPP

// https://github.com/aschrein/VulkII/blob/master/include/scene.hpp#L1617
namespace cpubvh {
// Based on
// https://interplayoflight.wordpress.com/2020/07/21/using-embree-generated-bvh-trees-for-gpu-raytracing/
struct LeafNode;
struct Node {
    AABB aabb = {};

    virtual f32 sah() = 0;
    virtual ~Node() {}
    virtual u32   Getnum_children() { return u32(0); }
    virtual Node *GetChild(u32 i) { return NULL; }
    virtual bool  IsLeaf() { return false; }
    bool          AnyHit(Ray const &ray, std::function<bool(Node *)> fn) {
        if (IsLeaf())
            if (fn(this)) return true;
        ifor(Getnum_children()) if (GetChild(i) && GetChild(i)->aabb.ray_test(ray.o, f32(1.0) / ray.d) && GetChild(i)->AnyHit(ray, fn)) return true;
        return false;
    }
    bool CheckAny(f32x3 p) {
        if (IsLeaf())
            if (aabb.contains(p)) return true;
        ifor(Getnum_children()) if (GetChild(i) && GetChild(i)->CheckAny(p)) return true;
        return false;
    }
};
struct InnerNode : public Node {
    u32    num_children = u32(0);
    Node **children     = {};
    bool   sah_dirty    = true;
    f32    sah_cache    = f32(0.0);

    ~InnerNode() override {}
    InnerNode(Node **_children, u32 _num_children) : num_children(_num_children), children(_children) {}
    u32   Getnum_children() override { return num_children; }
    Node *GetChild(u32 i) override { return children[i]; }
    f32   sah() override {
        if (!sah_dirty) return sah_cache;
        assert(num_children != u32(0));
        AABB b   = children[0]->aabb;
        f32  sum = f32(0.0);
        ifor(num_children) {
            sum += children[i]->aabb.area() * children[i]->sah();
            b.expand(children[i]->aabb);
        }
        sah_cache = f32(1.0) + sum / std::max(sum * f32(1.0e-6), b.area());
        sah_dirty = false;
        return sah_cache;
    }
};
struct LeafNode : public Node {
    u32 primitive_idx = 0;

    LeafNode *GetAsLeaf() { return this; }
    bool      IsLeaf() override { return true; }
    float     sah() override { return f32(1.0); }
    ~LeafNode() override {}
    LeafNode(u32 primitive_idx, const AABB &aabb) : primitive_idx(primitive_idx) { this->aabb = aabb; }
};
class BVH {

private:
    RTCDevice device = {};

    static void *CreateLeaf(RTCThreadLocalAllocator alloc, const RTCBuildPrimitive *prims, u64 numPrims, void *user_ptr) {
        assert(numPrims == 1);
        void *ptr  = rtcThreadLocalAlloc(alloc, sizeof(LeafNode), u64(16));
        AABB  aabb = {};
        aabb.lo    = f32x3(prims->lower_x, prims->lower_y, prims->lower_z);
        aabb.hi    = f32x3(prims->upper_x, prims->upper_y, prims->upper_z);
        return (void *)new (ptr) LeafNode(prims->primID, aabb);
    }
    static void *CreateNode(RTCThreadLocalAllocator alloc, unsigned int num_children, void *user_ptr) {
        void  *ptr            = rtcThreadLocalAlloc(alloc, sizeof(InnerNode), u64(16));
        Node **children_array = (Node **)rtcThreadLocalAlloc(alloc, sizeof(InnerNode *) * num_children, u64(16));
        return (void *)new (ptr) InnerNode(children_array, num_children);
    }
    static void SetChildren(void *nodePtr, void **childPtr, unsigned int num_children, void *user_ptr) {
        ifor(num_children)((InnerNode *)nodePtr)->children[i] = (Node *)childPtr[i];
    }
    static void SetBounds(void *nodePtr, const RTCBounds **bounds, unsigned int num_children, void *user_ptr) {
        assert(num_children > u32(1));
        ((Node *)nodePtr)->aabb.lo = f32x3(bounds[0]->lower_x, bounds[0]->lower_y, bounds[0]->lower_z);
        ((Node *)nodePtr)->aabb.hi = f32x3(bounds[0]->upper_x, bounds[0]->upper_y, bounds[0]->upper_z);
        ifor(num_children) {
            ((Node *)nodePtr)->aabb.expand(f32x3(bounds[i]->lower_x, bounds[i]->lower_y, bounds[i]->lower_z));
            ((Node *)nodePtr)->aabb.expand(f32x3(bounds[i]->upper_x, bounds[i]->upper_y, bounds[i]->upper_z));
        }
    }
    static void SplitPrimitive(const RTCBuildPrimitive *prim, unsigned int dim, float pos, RTCBounds *lprim, RTCBounds *rprim, void *user_ptr) {
        assert(dim < 3);
        *(RTCBuildPrimitive *)lprim = *(RTCBuildPrimitive *)prim;
        *(RTCBuildPrimitive *)rprim = *(RTCBuildPrimitive *)prim;
        (&lprim->upper_x)[dim]      = pos;
        (&rprim->lower_x)[dim]      = pos;
    }

public:
    void Init() { device = rtcNewDevice(NULL); }
    void Release() { rtcReleaseDevice(device); }
    struct BVHResult {
        RTCBVH bvh  = {};
        Node  *root = {};

        void Release() {
            if (bvh) {
                rtcReleaseBVH(bvh);
            }
            bvh  = {};
            root = NULL;
        }
        bool IsValid() const { return bvh && root; }
    };
    BVHResult Build(AABB *elems, u64 num_elems) {
        u64                            extra_nodes = num_elems;
        u64                            num_nodes   = num_elems;
        u64                            capacity    = num_elems + extra_nodes;
        std::vector<RTCBuildPrimitive> prims       = {};
        prims.resize(capacity);
        ifor(num_elems) {
            RTCBuildPrimitive prim = {};
            AABB              aabb = elems[i];
            prim.lower_x           = aabb.lo.x;
            prim.lower_y           = aabb.lo.y;
            prim.lower_z           = aabb.lo.z;
            prim.upper_x           = aabb.hi.x;
            prim.upper_y           = aabb.hi.y;
            prim.upper_z           = aabb.hi.z;
            prim.geomID            = u32(0);
            prim.primID            = i; // primitive_ids[i];
            prims[i]               = prim;
        }

        RTCBVH            bvh            = rtcNewBVH(device);
        RTCBuildArguments arguments      = rtcDefaultBuildArguments();
        arguments.byteSize               = sizeof(arguments);
        arguments.buildFlags             = RTC_BUILD_FLAG_NONE;
        arguments.buildQuality           = RTC_BUILD_QUALITY_LOW;
        arguments.maxBranchingFactor     = u32(4);
        arguments.maxDepth               = u32(1024);
        arguments.sahBlockSize           = u32(1);
        arguments.minLeafSize            = u32(1);
        arguments.maxLeafSize            = u32(1);
        arguments.traversalCost          = f32(1.0);
        arguments.intersectionCost       = f32(1.0);
        arguments.bvh                    = bvh;
        arguments.primitives             = &prims[0];
        arguments.primitiveCount         = num_nodes;
        arguments.primitiveArrayCapacity = capacity;
        arguments.createNode             = CreateNode;
        arguments.setNodeChildren        = SetChildren;
        arguments.setNodeBounds          = SetBounds;
        arguments.createLeaf             = CreateLeaf;
        arguments.splitPrimitive         = SplitPrimitive;
        arguments.buildProgress          = nullptr;
        arguments.user_ptr               = nullptr;
        BVHResult out                    = {};
        out.bvh                          = bvh;
        out.root                         = (Node *)rtcBuildBVH(&arguments);
        if (out.root == NULL) {
            fprintf(stdout, "[ERROR] Embree device error code: %i\n", rtcGetDeviceError(device));
        }
        assert(out.root && "Might be not enough max_depth");

        return out;
    }
};
} // namespace cpubvh

#endif // EMBREE_HPP