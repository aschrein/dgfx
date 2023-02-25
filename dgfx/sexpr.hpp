
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

namespace sexpr {
struct SNode {
    StringRef symbol  = {};
    SNode *   child   = NULL;
    SNode *   next    = NULL;
    i32       id      = i32(0);
    bool      quoted  = false;
    bool      squoted = false;

    void toString(String_Builder &sb) {
        if (quoted) sb.Putf("\"\"\"");
        sb.PutStr(symbol);
        if (quoted) sb.Putf("\"\"\"");
        if (child) {
            sb.Putf(" (");
            child->toString(sb);
            sb.Putf(")");
        }
        if (next) {
            next->toString(sb);
        }
    }
    StringRef GetSymbol() {
        ASSERT_ALWAYS(IsNonEmpty());
        return symbol;
    }
    StringRef GetUmbrellaString() {
        StringRef out = symbol;
        if (child != NULL) {
            StringRef th = child->GetUmbrellaString();
            if (out.ptr == NULL) out.ptr = th.ptr;
            out.len += (size_t)(th.ptr - out.ptr) - out.len + th.len;
        }
        if (next != NULL) {
            StringRef th = next->GetUmbrellaString();
            if (out.ptr == NULL) out.ptr = th.ptr;
            out.len += (size_t)(th.ptr - out.ptr) - out.len + th.len;
        }
        return out;
    }
    bool IsInt() {
        i32 res = i32(0);
        return ::parse_decimal_int(symbol.ptr, (i32)symbol.len, &res);
    }
    bool IsF32() {
        f32 res = f32(0.0);
        return ::ParseFloat(symbol.ptr, (i32)symbol.len, &res);
    }
    i32 ParseInt() {
        i32 res = i32(0);
        ASSERT_ALWAYS(::parse_decimal_int(symbol.ptr, (i32)symbol.len, &res));
        return res;
    }
    f32 ParseFloat() {
        f32 res = f32(0.0);
        ASSERT_ALWAYS(::ParseFloat(symbol.ptr, (i32)symbol.len, &res));
        return res;
    }
    bool IsNonEmpty() { return symbol.ptr != 0 && symbol.len != 0; }
    bool CmpSymbol(char const *str) {
        if (symbol.ptr == NULL) return false;
        return symbol == stref_s(str);
    }
    bool HasChild(char const *name) { return child != NULL && child->CmpSymbol(name); }
    template <typename T>
    void MatchChildren(char const *name, T on_match) {
        if (child != NULL) {
            if (child->CmpSymbol(name)) {
                on_match(child);
            }
            child->MatchChildren(name, on_match);
        }
        if (next != NULL) {
            next->MatchChildren(name, on_match);
        }
    }
    SNode *Get(u32 i) {
        SNode *cur = this;
        while (i != 0) {
            if (cur == NULL) return NULL;
            cur = cur->next;
            i -= 1;
        }
        return cur;
    }
    int Dump(u32 indent = 0) const {
        ifor(indent) fprintf(stdout, " ");
        if (symbol.ptr != NULL) {
            fprintf(stdout, "%.*s\n", (i32)symbol.len, symbol.ptr);
        } else {
            fprintf(stdout, "$\n");
        }
        if (child != NULL) {
            child->Dump(indent + 2);
        }
        if (next != NULL) {
            next->Dump(indent);
        }
        fflush(stdout);
        return 0;
    }
    void DumpDotGraph() {
        SNode *root     = this;
        FILE * dotgraph = fopen("list.dot", "wb");
        fprintf(dotgraph, "digraph {\n");
        fprintf(dotgraph, "node [shape=record];\n");
        tl_alloc_tmp_enter();
        defer(tl_alloc_tmp_exit());
        SNode **stack        = (SNode **)tl_alloc_tmp(sizeof(SNode *) * (1 << 10));
        u32     stack_cursor = 0;
        SNode * cur          = root;
        u32     null_id      = u32(0xffff);
        while (cur != NULL || stack_cursor != 0) {
            if (cur == NULL) {
                cur = stack[--stack_cursor];
            }
            ASSERT_ALWAYS(cur != NULL);
            if (cur->symbol.ptr != NULL) {
                ASSERT_ALWAYS(cur->symbol.len != 0);
                fprintf(dotgraph, "%i [label = \"%.*s\", shape = record];\n", cur->id, (int)cur->symbol.len, cur->symbol.ptr);
            } else {
                fprintf(dotgraph, "%i [label = \"$\", shape = record, color=red];\n", cur->id);
            }
            if (cur->next == NULL) {
                fprintf(dotgraph, "%i [label = \"nil\", shape = record, color=blue];\n", null_id);
                fprintf(dotgraph, "%i -> %i [label = \"next\"];\n", cur->id, null_id);
                null_id++;
            } else
                fprintf(dotgraph, "%i -> %i [label = \"next\"];\n", cur->id, cur->next->id);

            if (cur->child != NULL) {
                if (cur->next != NULL) stack[stack_cursor++] = cur->next;
                fprintf(dotgraph, "%i -> %i [label = \"child\"];\n", cur->id, cur->child->id);
                cur = cur->child;
            } else {
                fprintf(dotgraph, "%i [label = \"nil\", shape = record, color=blue];\n", null_id);
                fprintf(dotgraph, "%i -> %i [label = \"child\"];\n", cur->id, null_id);
                null_id++;

                cur = cur->next;
            }
        }
        fprintf(dotgraph, "}\n");
        fflush(dotgraph);
        fclose(dotgraph);
    }
    static SNode *Parse(StringRef text, char const **end_of_list = NULL) {
        SNode * root         = tl_alloc_tmp_init<SNode>();
        SNode * cur          = root;
        SNode **stack        = tl_alloc_tmp<SNode *>((1 << 8));
        u32     stack_cursor = 0;
        enum class State : char {
            UNDEFINED = 0,
            SAW_QUOTE,
            SAW_LPAREN,
            SAW_RPAREN,
            SAW_PRINTABLE,
            SAW_SEPARATOR,
            SAW_SEMICOLON,
            SAW_QUASIQUOTE,
        };
        u32   i  = 0;
        u32   id = 1;
        State state_table[0x100];
        memset(state_table, 0, sizeof(state_table));
        for (u8 j = 0x20; j <= 0x7f; j++) state_table[j] = State::SAW_PRINTABLE;
        state_table[(u32)'(']  = State::SAW_LPAREN;
        state_table[(u32)')']  = State::SAW_RPAREN;
        state_table[(u32)'"']  = State::SAW_QUOTE;
        state_table[(u32)' ']  = State::SAW_SEPARATOR;
        state_table[(u32)'\n'] = State::SAW_SEPARATOR;
        state_table[(u32)'\t'] = State::SAW_SEPARATOR;
        state_table[(u32)'\r'] = State::SAW_SEPARATOR;
        state_table[(u32)';']  = State::SAW_SEMICOLON;
        state_table[(u32)'`']  = State::SAW_QUASIQUOTE;

        bool next_is_data = false;

        auto next_item = [&]() {
            SNode *next = tl_alloc_tmp_init<SNode>();
            next->id    = id++;
            if (cur != NULL) cur->next = next;
            cur = next;
        };

        auto push_item = [&]() {
            SNode *new_head   = tl_alloc_tmp_init<SNode>();
            new_head->squoted = next_is_data;
            new_head->id      = id++;
            if (cur != NULL) {
                stack[stack_cursor++] = cur;
                cur->child            = new_head;
            }
            cur = new_head;
        };

        auto pop_item = [&]() -> bool {
            if (stack_cursor == 0) {
                return false;
            }
            cur = stack[--stack_cursor];
            return true;
        };

        auto append_char = [&]() {
            if (cur->symbol.ptr == NULL) { // first character for that item
                cur->symbol.ptr = text.ptr + i;
            }
            cur->symbol.len++;
        };

        auto set_quoted = [&]() { cur->quoted = true; };

        auto cur_non_empty = [&]() { return cur != NULL && cur->symbol.len != 0; };
        auto cur_has_child = [&]() { return cur != NULL && cur->child != NULL; };

        i                = 0;
        State prev_state = State::UNDEFINED;

        while (i < text.len) {
            char c = text.ptr[i];
            if (c == '\0') break;
            State state = state_table[(u8)c];
            switch (state) {
            case State::UNDEFINED: {
                goto error_parsing;
            }
            case State::SAW_QUASIQUOTE: {
                assert(next_is_data == false);
                next_is_data = true;
                break;
            }
            case State::SAW_SEMICOLON: {
                next_is_data = false;
                i += 1;
                while (text.ptr[i] != '\n' && text.ptr[i] != '\0' && i != text.len) {
                    i += 1;
                }
                break;
            }
            case State::SAW_QUOTE: {
                next_is_data = false;
                if (cur_non_empty() || cur_has_child()) next_item();
                set_quoted();
                if (text.ptr[i + 1] == '"' && text.ptr[i + 2] == '"') {
                    i += 3;
                    while (text.ptr[i + 0] != '"' || //
                           text.ptr[i + 1] != '"' || //
                           text.ptr[i + 2] != '"') {
                        append_char();
                        i += 1;
                    }
                    i += 2;
                } else {
                    i += 1;
                    while (text.ptr[i] != '"') {
                        append_char();
                        i += 1;
                    }
                }
                break;
            }
            case State::SAW_LPAREN: {
                if (cur_has_child() || cur_non_empty()) next_item();
                push_item();
                next_is_data = false;
                break;
            }
            case State::SAW_RPAREN: {
                next_is_data = false;
                if (pop_item() == false) goto exit_loop;
                break;
            }
            case State::SAW_SEPARATOR: {
                next_is_data = false;
                break;
            }
            case State::SAW_PRINTABLE: {
                next_is_data = false;
                if (cur_has_child()) next_item();
                if (cur_non_empty() && prev_state != State::SAW_PRINTABLE) next_item();
                append_char();
                break;
            }
            }
            prev_state = state;
            i += 1;
        }
    exit_loop:
        (void)0;
        if (end_of_list != NULL) {
            *end_of_list = text.ptr + i + 1;
        }
        return root;
    error_parsing:
        return NULL;
    }
};
struct Symbol_Table {
    struct Value {
        enum class Value_t : i32 { UNKNOWN = 0, I32, F32, SYMBOL, BINDING, LAMBDA, SCOPE, MODE, ANY };
        i32 type     = i32(0);
        i32 any_type = i32(0);
        union {
            StringRef str;
            f32       f;
            i32       i;
            SNode *   list;
            void *    any;
        };

        void Release() { delete this; }
        void Dump() {
            fprintf(stdout, "Value; {\n");
            switch (type) {
            case (i32)Value_t::I32: {
                fprintf(stdout, "  i32: %i\n", i);
                break;
            }
            case (i32)Value_t::F32: {
                fprintf(stdout, "  f32: %f\n", f);
                break;
            }
            case (i32)Value_t::SYMBOL: {
                fprintf(stdout, "  sym: %.*s\n", STRF(str));
                break;
            }
            case (i32)Value_t::BINDING: {
                fprintf(stdout, "  bnd:\n");
                list->Dump(4);
                break;
            }
            case (i32)Value_t::LAMBDA: {
                fprintf(stdout, "  lmb:\n");
                list->Dump(4);
                break;
            }
            case (i32)Value_t::SCOPE: {
                fprintf(stdout, "  scp\n");
                break;
            }
            case (i32)Value_t::ANY: {
                fprintf(stdout, "  any\n");
                break;
            }
            case (i32)Value_t::MODE: {
                fprintf(stdout, "  mod\n");
                break;
            }
            default: UNIMPLEMENTED;
            }
            fprintf(stdout, "}\n");
            fflush(stdout);
        }
    };
    struct Symbol {
        StringRef name = {};
        Value *   val  = {};
    };
    struct Symbol_Frame {
        using Table_t       = std::unordered_map<StringRef, Value *>;
        Table_t       table = {};
        Symbol_Frame *prev  = {};
        void          Init() {
            table = {};
            prev  = NULL;
        }
        void Release() {
            for (auto &v : table) {
                v.second->Release();
            }
            table.clear();
            prev = NULL;
            delete this;
        }
        static Symbol_Frame *Create() {
            Symbol_Frame *o = new Symbol_Frame;
            o->Init();
            return o;
        }
        Value *Get(StringRef name) {
            auto it = table.find(name);
            if (it != table.end()) return it->second;
            return NULL;
        }
        void Insert(StringRef name, Value *val) {
            if (Value *old_val = Get(name)) {
                old_val->Release();
            }
            table[name] = val;
        }
    };
    std::vector<Symbol_Frame *> table_storage = {};
    Symbol_Frame *              tail          = {};
    Symbol_Frame *              head          = {};

    void Init() {
        table_storage.push_back(Symbol_Frame::Create());
        tail = table_storage[0];
        head = table_storage[0];
    }
    static Symbol_Table *Create() {
        Symbol_Table *o = new Symbol_Table;
        o->Init();
        return o;
    }
    void Release() {
        for (auto t : table_storage) {
            t->Release();
        }
        table_storage.clear();
        delete this;
    }
    Value *lookup_value(StringRef name) {
        Symbol_Frame *cur = tail;
        while (cur != NULL) {
            if (Value *val = cur->Get(name)) return val;
            cur = cur->prev;
        }
        return NULL;
    }
    Value *lookup_value(StringRef name, void *scope) {
        Symbol_Frame *cur = (Symbol_Frame *)scope;
        while (cur != NULL) {
            if (Value *val = cur->Get(name)) return val;
            cur = cur->prev;
        }
        return NULL;
    }
    void *get_scope() { return (void *)tail; }
    void  set_scope(void *scope) { tail = (Symbol_Frame *)scope; }
    void  enter_scope() {
        Symbol_Frame *new_table = Symbol_Frame::Create();
        table_storage.push_back(new_table);
        new_table->prev = tail;
        tail            = new_table;
    }
    void exit_scope() {
        Symbol_Frame *new_tail = tail->prev;
        assert(new_tail != NULL);
        tail->Release();
        table_storage.pop_back();
        tail = new_tail;
    }
    /*void Dump() {
        Symbol_Frame *cur = tail;
        while (cur != NULL) {
            fprintf(stdout, "--------new-table\n");
            cur->table.iter([&](Symbol_Frame::Table_t::Pair_t const &item) {
                fprintf(stdout, "symbol(\"%.*s\"):\n", STRF(item.key));
                item.value->Dump();
            });
            cur = cur->prev;
        }
    }*/
    void add_symbol(StringRef name, Value *val) { tail->Insert(name, val); }
};

//////////////////
// Global state //
//////////////////
#if 0
struct Evaluator_State {
    Pool<char>   string_storage;
    Pool<SNode>  list_storage;
    Pool<Value>  value_storage;
    Symbol_Table symbol_table;
    bool         eval_error = false;

    void Init() {
        string_storage = Pool<char>::create((1 << 20));
        list_storage   = Pool<SNode>::create((1 << 20));
        value_storage  = Pool<Value>::create((1 << 20));
        symbol_table.Init();
    }

    void Release() {
        string_storage.Release();
        list_storage.Release();
        value_storage.Release();
        symbol_table.Release();
    }

    void Reset() {
        string_storage.Reset();
        list_storage.Reset();
        value_storage.Reset();
        symbol_table.Release();
        symbol_table.Init();
    }

    void enter_scope() {
        string_storage.enter_scope();
        list_storage.enter_scope();
        value_storage.enter_scope();
        symbol_table.enter_scope();
    }

    void exit_scope() {
        string_storage.exit_scope();
        list_storage.exit_scope();
        value_storage.exit_scope();
        symbol_table.exit_scope();
    }

    Value *alloc_value() { return value_storage.alloc_zero(1); }

    StringRef move_cstr(StringRef old) {
        char     *new_ptr = string_storage.put(old.ptr, old.len + 1);
        StringRef new_ref = StringRef{new_ptr, old.len};
        new_ptr[old.len]  = '\0';
        return new_ref;
    }
};
struct Match {
    Value *val;
    bool   match;
    Match(Value *val) : val(val), match(true) {}
    Match(Value *val, bool match) : val(val), match(match) {}
    Value *unwrap() {
        if (!match) {
            ASSERT_ALWAYS(false);
            return NULL;
        }
        return val;
    }
};

struct Tmp_List_Allocator {
    SNode *alloc() {
        SNode *out = (SNode *)tl_alloc_tmp(sizeof(SNode));
        memset(out, 0, sizeof(SNode));
        return out;
    }
    char *alloc(size_t size) { return (char *)tl_alloc_tmp(size); }
};

struct IEvaluator;
typedef IEvaluator *(*Evaluator_Creator_t)();
#endif
static inline void push_warning(char const *fmt, ...) {
    fprintf(stdout, "[WARNING] ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);
}

static inline void push_error(char const *fmt, ...) {
    fprintf(stdout, "[ERROR] ");
    va_list args;
    va_start(args, fmt);
    vfprintf(stdout, fmt, args);
    va_end(args);
    fprintf(stdout, "\n");
    fflush(stdout);
}

#if 0

struct IEvaluator {
    Evaluator_State* state = NULL;
    IEvaluator* prev = NULL;
    virtual Match      eval(SNode*) = 0;
    virtual void       Release() = 0;
    static IEvaluator* get_head();
    static void        add_mode(StringRef name, Evaluator_Creator_t creat);
    static void        set_head(IEvaluator*);
    static IEvaluator* create_mode(StringRef name);
    static Match       global_eval(SNode* l) { return get_head()->eval(l); }
    Value* eval_unwrap(SNode* l) { return global_eval(l).unwrap(); }
    Value* eval_args(SNode* arg) {
        SNode* cur = arg;
        Value* last = NULL;
        while (cur != NULL) {
            Value* a = eval_unwrap(cur);
            if (a != NULL && a->type == (i32)Value::Value_t::BINDING) {
                a = eval_args(a->list);
            }
            cur = cur->next;
            last = a;
        }
        return last;
    }
    template <typename V>
    void eval_args_and_collect(SNode* l, V& values) {
        SNode* cur = l;
        while (cur != NULL) {
            Value* a = eval_unwrap(cur);
            if (a != NULL && a->type == (i32)Value::Value_t::BINDING) {
                eval_args_and_collect(a->list, values);
            }
            else {
                values.push(a);
            }
            cur = cur->next;
        }
    }
    Value* alloc_value() { return state->alloc_value(); }
    StringRef  move_cstr(StringRef old) { return state->move_cstr(old); }
    void        set_error() { state->eval_error = true; }
    bool        is_error() { return state->eval_error; }
    static void parse_and_eval(StringRef text) {
        Evaluator_State state;
        state.Init();
        defer(state.Release());

        struct List_Allocator {
            Evaluator_State* state;
            SNode* alloc() {
                SNode* out = state->list_storage.alloc_zero(1);
                return out;
            }
        } list_allocator;
        list_allocator.state = &state;
        SNode* root = SNode::Parse(text, list_allocator);
        if (root == NULL) {
            push_error("Couldn't Parse");
            return;
        }
        root->DumpDotGraph();

        IEvaluator::get_head()->state = &state;
        IEvaluator::get_head()->eval(root);
    }
};
#endif // 0

#define ASSERT_EVAL(x)                                                                                                                                                             \
    do {                                                                                                                                                                           \
        if (!(x)) {                                                                                                                                                                \
            set_error();                                                                                                                                                           \
            push_error(#x);                                                                                                                                                        \
            abort();                                                                                                                                                               \
            return NULL;                                                                                                                                                           \
        }                                                                                                                                                                          \
    } while (0)
#define CHECK_ERROR()                                                                                                                                                              \
    do {                                                                                                                                                                           \
        if (is_error()) {                                                                                                                                                          \
            abort();                                                                                                                                                               \
            return NULL;                                                                                                                                                           \
        }                                                                                                                                                                          \
    } while (0)

#ifdef SCRIPT_IMPL

#    define ALLOC_VAL() (Value *)alloc_value()
#    define CALL_EVAL(x)                                                                                                                                                           \
        eval_unwrap(x);                                                                                                                                                            \
        CHECK_ERROR()
#    define ASSERT_SMB(x) ASSERT_EVAL(x != NULL && x->type == (i32)Value::Value_t::SYMBOL);
#    define ASSERT_I32(x) ASSERT_EVAL(x != NULL && x->type == (i32)Value::Value_t::I32);
#    define ASSERT_F32(x) ASSERT_EVAL(x != NULL && x->type == (i32)Value::Value_t::F32);
#    define ASSERT_ANY(x) ASSERT_EVAL(x != NULL && x->type == (i32)Value::Value_t::ANY);

#    define EVAL_SMB(res, id)                                                                                                                                                      \
        Value *res = eval_unwrap(l->Get(id));                                                                                                                                      \
        ASSERT_SMB(res)
#    define EVAL_I32(res, id)                                                                                                                                                      \
        Value *res = eval_unwrap(l->Get(id));                                                                                                                                      \
        ASSERT_I32(res)
#    define EVAL_F32(res, id)                                                                                                                                                      \
        Value *res = eval_unwrap(l->Get(id));                                                                                                                                      \
        ASSERT_F32(res)
#    define EVAL_ANY(res, id)                                                                                                                                                      \
        Value *res = eval_unwrap(l->Get(id));                                                                                                                                      \
        ASSERT_ANY(res)

struct Default_Evaluator final : public IEvaluator {
    void               Init() {}
    Default_Evaluator *create() {
        Default_Evaluator *out = new Default_Evaluator;
        out->Init();
        return out;
    }
    void  Release() override { delete this; }
    Match eval(SNode *l) override {
        if (l == NULL) return NULL;
        TMP_STORAGE_SCOPE;
        if (l->child != NULL) {
            ASSERT_EVAL(!l->IsNonEmpty());
            return global_eval(l->child);
        } else if (l->IsNonEmpty()) {
            i32  imm32;
            f32  immf32;
            bool is_imm32  = !l->quoted && parse_decimal_int(l->symbol.ptr, (i32)l->symbol.len, &imm32);
            bool is_immf32 = !l->quoted && ParseFloat(l->symbol.ptr, (i32)l->symbol.len, &immf32);
            if (is_imm32) {
                Value *new_val = ALLOC_VAL();
                new_val->i     = imm32;
                new_val->type  = (i32)Value::Value_t::I32;
                return new_val;
            } else if (is_immf32) {
                Value *new_val = ALLOC_VAL();
                new_val->f     = immf32;
                new_val->type  = (i32)Value::Value_t::F32;
                return new_val;
            } else if (l->CmpSymbol("for-range")) {
                SNode *name_l = l->next;
                ASSERT_EVAL(name_l->IsNonEmpty());
                StringRef name = name_l->symbol;
                Value *   lb   = CALL_EVAL(l->Get(2));
                ASSERT_EVAL(lb != NULL && lb->type == (i32)Value::Value_t::I32);
                Value *ub = CALL_EVAL(l->Get(3));
                ASSERT_EVAL(ub != NULL && ub->type == (i32)Value::Value_t::I32);
                Value *new_val = ALLOC_VAL();
                new_val->i     = 0;
                new_val->type  = (i32)Value::Value_t::I32;
                for (i32 i = lb->i; i < ub->i; i++) {
                    state->symbol_table.enter_scope();
                    new_val->i = i;
                    state->symbol_table.add_symbol(name, new_val);
                    defer(state->symbol_table.exit_scope());
                    eval_args(l->Get(4));
                }
                return NULL;
            } else if (l->CmpSymbol("for-items")) {
                Value *name = CALL_EVAL(l->next);
                ASSERT_EVAL(name->type == (i32)Value::Value_t::SYMBOL);
                SmallArray<Value *, 8> items;
                items.Init();
                defer(items.Release());
                eval_args_and_collect(l->next->next->child, items);
                ito(items.size) {
                    state->symbol_table.enter_scope();
                    state->symbol_table.add_symbol(name->str, items[i]);
                    defer(state->symbol_table.exit_scope());
                    eval_args(l->Get(3));
                }
                return NULL;
            } else if (l->CmpSymbol("if")) {
                EVAL_I32(cond, 1);
                state->symbol_table.enter_scope();
                defer(state->symbol_table.exit_scope());
                if (cond->i != 0) {
                    Value *val = CALL_EVAL(l->Get(2));
                    return val;
                } else {
                    Value *val = CALL_EVAL(l->Get(3));
                    return val;
                }
            } else if (l->CmpSymbol("add-mode")) {
                EVAL_SMB(name, 1);
                state->symbol_table.enter_scope();
                defer(state->symbol_table.exit_scope());
                IEvaluator *mode = IEvaluator::create_mode(name->str);
                ASSERT_EVAL(mode != NULL);
                IEvaluator *old_head = IEvaluator::get_head();
                IEvaluator::set_head(mode);
                eval_args(l->Get(2));
                IEvaluator::set_head(old_head);
                mode->Release();
                return NULL;
            } else if (l->CmpSymbol("lambda")) {
                Value *new_val = ALLOC_VAL();
                new_val->list  = l->next;
                new_val->type  = (i32)Value::Value_t::LAMBDA;
                return new_val;
            } else if (l->CmpSymbol("scope")) {
                state->symbol_table.enter_scope();
                defer(state->symbol_table.exit_scope());
                return eval_args(l->next);
            } else if (l->CmpSymbol("add")) {
                SmallArray<Value *, 2> args;
                args.Init();
                defer(args.Release());
                eval_args_and_collect(l->next, args);
                ASSERT_EVAL(args.size == 2);
                Value *op1 = args[0];
                ASSERT_EVAL(op1 != NULL);
                Value *op2 = args[1];
                ASSERT_EVAL(op2 != NULL);
                ASSERT_EVAL(op1->type == op2->type);
                if (op1->type == (i32)Value::Value_t::I32) {
                    Value *new_val = ALLOC_VAL();
                    new_val->i     = op1->i + op2->i;
                    new_val->type  = (i32)Value::Value_t::I32;
                    return new_val;
                } else if (op1->type == (i32)Value::Value_t::F32) {
                    Value *new_val = ALLOC_VAL();
                    new_val->f     = op1->f + op2->f;
                    new_val->type  = (i32)Value::Value_t::F32;
                    return new_val;
                } else {
                    ASSERT_EVAL(false && "add: unsopported operand types");
                }
                return NULL;
            } else if (l->CmpSymbol("sub")) {
                SmallArray<Value *, 2> args;
                args.Init();
                defer(args.Release());
                eval_args_and_collect(l->next, args);
                ASSERT_EVAL(args.size == 2);
                Value *op1 = args[0];
                ASSERT_EVAL(op1 != NULL);
                Value *op2 = args[1];
                ASSERT_EVAL(op2 != NULL);
                ASSERT_EVAL(op1->type == op2->type);
                if (op1->type == (i32)Value::Value_t::I32) {
                    Value *new_val = ALLOC_VAL();
                    new_val->i     = op1->i - op2->i;
                    new_val->type  = (i32)Value::Value_t::I32;
                    return new_val;
                } else if (op1->type == (i32)Value::Value_t::F32) {
                    Value *new_val = ALLOC_VAL();
                    new_val->f     = op1->f - op2->f;
                    new_val->type  = (i32)Value::Value_t::F32;
                    return new_val;
                } else {
                    ASSERT_EVAL(false && "sub: unsopported operand types");
                }
                return NULL;
            } else if (l->CmpSymbol("mul")) {
                SmallArray<Value *, 2> args;
                args.Init();
                defer(args.Release());
                eval_args_and_collect(l->next, args);
                ASSERT_EVAL(args.size == 2);
                Value *op1 = args[0];
                ASSERT_EVAL(op1 != NULL);
                Value *op2 = args[1];
                ASSERT_EVAL(op2 != NULL);
                ASSERT_EVAL(op1->type == op2->type);
                if (op1->type == (i32)Value::Value_t::I32) {
                    Value *new_val = ALLOC_VAL();
                    new_val->i     = op1->i * op2->i;
                    new_val->type  = (i32)Value::Value_t::I32;
                    return new_val;
                } else if (op1->type == (i32)Value::Value_t::F32) {
                    Value *new_val = ALLOC_VAL();
                    new_val->f     = op1->f * op2->f;
                    new_val->type  = (i32)Value::Value_t::F32;
                    return new_val;
                } else {
                    ASSERT_EVAL(false && "mul: unsopported operand types");
                }
                return NULL;
            } else if (l->CmpSymbol("cmp")) {
                SNode *                mode = l->next;
                SmallArray<Value *, 2> args;
                args.Init();
                defer(args.Release());
                eval_args_and_collect(mode->next, args);
                ASSERT_EVAL(args.size == 2);
                Value *op1 = args[0];
                ASSERT_EVAL(op1 != NULL);
                Value *op2 = args[1];
                ASSERT_EVAL(op2 != NULL);
                ASSERT_EVAL(op1->type == op2->type);
                if (mode->CmpSymbol("lt")) {
                    if (op1->type == (i32)Value::Value_t::I32) {
                        Value *new_val = ALLOC_VAL();
                        new_val->i     = op1->i < op2->i ? 1 : 0;
                        new_val->type  = (i32)Value::Value_t::I32;
                        return new_val;
                    } else if (op1->type == (i32)Value::Value_t::F32) {
                        Value *new_val = ALLOC_VAL();
                        new_val->i     = op1->f < op2->f ? 1 : 0;
                        new_val->type  = (i32)Value::Value_t::I32;
                        return new_val;
                    } else {
                        ASSERT_EVAL(false && "cmp: unsopported operand types");
                    }
                } else if (mode->CmpSymbol("eq")) {
                    if (op1->type == (i32)Value::Value_t::I32) {
                        Value *new_val = ALLOC_VAL();
                        new_val->i     = op1->i == op2->i ? 1 : 0;
                        new_val->type  = (i32)Value::Value_t::I32;
                        return new_val;
                    } else if (op1->type == (i32)Value::Value_t::F32) {
                        Value *new_val = ALLOC_VAL();
                        new_val->i     = op1->f == op2->f ? 1 : 0;
                        new_val->type  = (i32)Value::Value_t::I32;
                        return new_val;
                    } else {
                        ASSERT_EVAL(false && "cmp: unsopported operand types");
                    }
                } else {
                    ASSERT_EVAL(false && "cmp: unsopported op");
                }
                return NULL;
            } else if (l->CmpSymbol("let")) {
                SNode *name = l->next;
                ASSERT_EVAL(name->IsNonEmpty());
                Value *val = CALL_EVAL(l->Get(2));
                state->symbol_table.add_symbol(name->symbol, val);
                return val;
            } else if (l->CmpSymbol("Get-scope")) {
                Value *new_val = ALLOC_VAL();
                new_val->any   = state->symbol_table.get_scope();
                new_val->type  = (i32)Value::Value_t::SCOPE;
                return new_val;
            } else if (l->CmpSymbol("set-scope")) {
                Value *val = CALL_EVAL(l->Get(1));
                ASSERT_EVAL(val != NULL && val->type == (i32)Value::Value_t::SCOPE);
                void *old_scope = state->symbol_table.get_scope();
                state->symbol_table.set_scope(val->any);
                state->symbol_table.enter_scope();
                defer({
                    state->symbol_table.exit_scope();
                    state->symbol_table.set_scope(old_scope);
                });
                { // Preserve list
                    SNode *cur = l->Get(2)->child;
                    while (cur != NULL) {
                        if (cur->IsNonEmpty()) {
                            state->symbol_table.add_symbol(cur->symbol, state->symbol_table.lookup_value(cur->symbol, old_scope));
                        }
                        cur = cur->next;
                    }
                }
                return eval_args(l->Get(3));
            } else if (l->CmpSymbol("Get-mode")) {
                Value *new_val = ALLOC_VAL();
                new_val->any   = get_head();
                new_val->type  = (i32)Value::Value_t::MODE;
                return new_val;
            } else if (l->CmpSymbol("set-mode")) {
                Value *val = CALL_EVAL(l->Get(1));
                ASSERT_EVAL(val != NULL && val->type == (i32)Value::Value_t::MODE);
                IEvaluator *old_mode = get_head();
                set_head((IEvaluator *)val->any);
                defer(set_head(old_mode););
                return eval_args(l->Get(2));
            } else if (l->CmpSymbol("quote")) {
                Value *new_val = ALLOC_VAL();
                new_val->list  = l->next;
                new_val->type  = (i32)Value::Value_t::BINDING;
                return new_val;
            } else if (l->CmpSymbol("deref")) {
                return state->symbol_table.lookup_value(l->next->symbol);
            } else if (l->CmpSymbol("unbind")) {
                ASSERT_EVAL(l->next->IsNonEmpty());
                Value *sym = state->symbol_table.lookup_value(l->next->symbol);
                ASSERT_EVAL(sym != NULL && sym->type == (i32)Value::Value_t::BINDING);
                return global_eval(sym->list);
            } else if (l->CmpSymbol("unquote")) {
                ASSERT_EVAL(l->next->IsNonEmpty());
                Value *sym = state->symbol_table.lookup_value(l->next->symbol);
                ASSERT_EVAL(sym != NULL && sym->type == (i32)Value::Value_t::BINDING);
                ASSERT_EVAL(sym->list->IsNonEmpty());
                Value *new_val = ALLOC_VAL();
                new_val->str   = sym->list->symbol;
                new_val->type  = (i32)Value::Value_t::SYMBOL;
                return new_val;
            } else if (l->CmpSymbol("nil")) {
                return NULL;
            } else if (l->CmpSymbol("print")) {
                EVAL_SMB(str, 1);
                fprintf(stdout, "%.*s\n", STRF(str->str));
                return NULL;
            } else if (l->CmpSymbol("format")) {
                // state->symbol_table.Dump();
                SmallArray<Value *, 4> args;
                args.Init();
                defer({ args.Release(); });
                eval_args_and_collect(l->next, args);
                Value *fmt    = args[0];
                u32    cur_id = 1;
                {
                    char *      tmp_buf = (char *)tl_alloc_tmp(0x100);
                    u32         cursor  = 0;
                    char const *c       = fmt->str.ptr;
                    char const *end     = fmt->str.ptr + fmt->str.len;
                    while (c != end) {
                        if (c[0] == '%') {
                            if (c + 1 == end) {
                                ASSERT_EVAL(false && "[format] Format string ends with %%");
                            }
                            if (cur_id == args.size) {
                                ASSERT_EVAL(false && "[format] Not enough arguments");
                            } else {
                                i32    num_chars = 0;
                                Value *val       = args[cur_id];
                                if (c[1] == 'i') {
                                    ASSERT_EVAL(val != NULL && val->type == (i32)Value::Value_t::I32);
                                    num_chars = sprintf(tmp_buf + cursor, "%i", val->i);
                                } else if (c[1] == 'f') {
                                    ASSERT_EVAL(val != NULL && val->type == (i32)Value::Value_t::F32);
                                    num_chars = sprintf(tmp_buf + cursor, "%f", val->f);
                                } else if (c[1] == 's') {
                                    ASSERT_EVAL(val != NULL && val->type == (i32)Value::Value_t::SYMBOL);
                                    num_chars = sprintf(tmp_buf + cursor, "%.*s", (i32)val->str.len, val->str.ptr);
                                } else {
                                    ASSERT_EVAL(false && "[format]  Unknown format");
                                }
                                if (num_chars < 0) {
                                    ASSERT_EVAL(false && "[format] Blimey!");
                                }
                                if (num_chars > 0x100) {
                                    ASSERT_EVAL(false && "[format] Format buffer overflow!");
                                }
                                cursor += num_chars;
                            }
                            cur_id += 1;
                            c += 1;
                        } else {
                            tmp_buf[cursor++] = c[0];
                        }
                        c += 1;
                    }
                    tmp_buf[cursor] = '\0';
                    Value *new_val  = ALLOC_VAL();
                    new_val->str    = move_cstr(stref_s(tmp_buf));
                    new_val->type   = (i32)Value::Value_t::SYMBOL;
                    return new_val;
                }
            } else {
                ASSERT_EVAL(l->IsNonEmpty());
                Value *sym = state->symbol_table.lookup_value(l->symbol);
                if (sym != NULL) {
                    if (sym->type == (i32)Value::Value_t::LAMBDA) {
                        ASSERT_EVAL(sym->list->child != NULL);
                        SNode *lambda   = sym->list; // Try to evaluate
                        SNode *arg_name = lambda->child;
                        SNode *arg_val  = l->next;
                        state->symbol_table.enter_scope();
                        defer(state->symbol_table.exit_scope());
                        bool saw_vararg = false;
                        while (arg_name != NULL && arg_name->IsNonEmpty()) { // Bind arguments
                            ASSERT_EVAL(!saw_vararg && "vararg must be the last argument");
                            ASSERT_EVAL(arg_val != NULL);
                            ASSERT_EVAL(arg_name->IsNonEmpty());
                            if (arg_name->CmpSymbol("...")) {
                                Value *new_val = ALLOC_VAL();
                                new_val->list  = arg_val;
                                new_val->type  = (i32)Value::Value_t::BINDING;
                                state->symbol_table.add_symbol(arg_name->symbol, new_val);
                                saw_vararg = true;
                            } else {
                                Value *val = CALL_EVAL(arg_val);
                                state->symbol_table.add_symbol(arg_name->symbol, val);
                            }
                            arg_name = arg_name->next;
                            arg_val  = arg_val->next;
                        }
                        return eval_args(lambda->next);
                    } else if (sym->type == (i32)Value::Value_t::BINDING) {
                        //            Value *val = CALL_EVAL(sym->list);
                        Value *val = sym;
                        return val;
                    }
                    return sym;
                }
                Value *new_val = ALLOC_VAL();
                new_val->str   = l->symbol;
                new_val->type  = (i32)Value::Value_t::SYMBOL;
                return new_val;
            }
        }
        TRAP;
    }
};

IEvaluator *g_head = NULL;

IEvaluator *IEvaluator::get_head() {
    if (g_head == NULL) {
        Default_Evaluator *head = new Default_Evaluator();
        head->Init();
        g_head = head;
    }
    return g_head;
}

void IEvaluator::set_head(IEvaluator *newhead) { g_head = newhead; }

Hash_Table<StringRef, Evaluator_Creator_t> &get_factory_table() {
    static Hash_Table<StringRef, Evaluator_Creator_t> table;
    static int                                        _init = [&] {
        table.Init();
        return 0;
    }();
    (void)_init;
    return table;
}

void IEvaluator::add_mode(StringRef name, Evaluator_Creator_t creat) { get_factory_table().insert(name, creat); }

IEvaluator *IEvaluator::create_mode(StringRef name) {
    if (get_factory_table().contains(name)) {
        IEvaluator *out = (*get_factory_table().Get(name))();
        out->prev       = get_head();
        out->state      = get_head()->state;
        return out;
    }
    return NULL;
}
#endif // SCRIPT_IMPL_GUARD

} // namespace sexpr

namespace TopGSL {

enum class Type {
    UNKNOWN = 0,
    BINOP,
    CALL,
    VALUE,
    SYMBOL,
    SCOPE,
    DEFUN,
    DECL,
    IF,
};
enum class ValueType {
    UNKNOWN = 0,
    I8,
    U8,
    I16,
    U16,
    I32,
    I32X2,
    I32X3,
    I32X4,
    U32,
    U32X2,
    U32X3,
    U32X4,
    I64,
    U64,
    F32,
    F32X2,
    F32X3,
    F32X4,
    F64,
    F16,
    F16X2,
    F16X3,
    F16X4,
    BOOL,
};
struct Value {
    ValueType type;
    union {
        double v_f64;
        float  v_f32;
        u32    v_u32;
        u64    v_u64;
        i32    v_i32;
        i64    v_i64;
        bool   v_bool;
    };
};
struct Expr {
    union {
        Expr *child;
        Expr *lhs;
        Expr *cond;
        Value value;
    };
    StringRef token;
    union {
        Expr *rhs;
        Expr *argv;
        Expr *then_scope;
    };
    union {
        Expr *body_scope;
        Expr *else_scope;
    };
    Expr *index_expr; // inside '['  ']'
    Type  type;
    char  lscope, rscope;

    void toString(String_Builder &sb) {
        if (type == Type::BINOP) {
            if (lhs) lhs->toString(sb);
            sb.PutStr(token);
            if (rhs) rhs->toString(sb);
        } else if (type == Type::SCOPE) {
            sb.Putf("%c", lscope);
            child->toString(sb);
            sb.Putf("%c", rscope);
        } else if (type == Type::CALL) {
            sb.PutStr(token);
            child->toString(sb);
        } else if (type == Type::SYMBOL) {
            sb.PutStr(token);
        } else if (type == Type::VALUE) {
            sb.PutStr(token);
        } else if (type == Type::DEFUN) {
            sb.Putf("defun ");
            sb.PutStr(token);
            if (argv) {
                sb.Putf("(");
                argv->toString(sb);
                sb.Putf(")");
            }
            if (body_scope) {
                sb.Putf("{");
                body_scope->toString(sb);
                sb.Putf("}");
            }
        } else {
            UNIMPLEMENTED;
        }
        if (index_expr) {
            sb.Putf("[");
            index_expr->toString(sb);
            sb.Putf("]");
        }
    }
};
static Expr *tmp_alloc_expr() {
    Expr *out = (Expr *)tl_alloc_tmp(sizeof(Expr));
    memset(out, 0, sizeof(Expr));
    return out;
}
static bool isLiteral(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
static bool isPrintable(char c) { return c >= 0x20 && c <= 0x7F; }
static bool isNumeral(char c) { return c >= '0' && c <= '9'; }
static void skipSpaces(char const *&cursor) {
    while (*cursor == ' ' || *cursor == '\n' || *cursor == '\t' || *cursor == '\r') {
        cursor++;
    }
}
static char        single_char_ops[] = {'+', '-', '*', '/', ',', '>', '<', '^', ';', '=', '.'};
static char const *two_char_ops[]    = {"<=", ">=", "!=", "+=", "-=", "*=", "/=", "<-"};
static int         precedence[0x100];
static int         prec_init = [] {
    ifor(ARRAYSIZE(precedence)) precedence[i] = -1;
    // semicolon is used to chain expressions
    precedence[(int)';'] = 0;
    precedence[(int)','] = 1;
    // precedence[(int)':'] = 2;
    precedence[(int)'='] = 3;
    precedence[(int)'<'] = 5;
    precedence[(int)'>'] = 5;
    precedence[(int)'+'] = 10;
    precedence[(int)'-'] = 10;
    precedence[(int)'*'] = 20;
    precedence[(int)'/'] = 20;
    precedence[(int)'^'] = 30;
    precedence[(int)'.'] = 50;
    return 0;
}();
static int getPrecedence(StringRef token) {
    if (token.len == 1) {
        assert(precedence[(int)token.ptr[0]] >= 0);
        return precedence[(int)token.ptr[0]];
    } else if (token.len == 2) {
        if (token.eq("<=")    //
            || token.eq(">=") //
            || token.eq("!=") //
            || token.eq("+=") //
            || token.eq("-=") //
            || token.eq("*=") //
            || token.eq("/=") //
            || token.eq("^=") //
        )
            return 5;
        if (token.eq("<-")) return 3;
    }
    UNIMPLEMENTED;
}
static bool isLogicOp(StringRef c) { return c.eq(">") || c.eq("<") || c.eq("=") || c.eq(">=") || c.eq("<=") || c.eq("!="); }
static bool isArithmeticOp(StringRef c) { return c.ptr[0] == '+' || c.ptr[0] == '-' || c.ptr[0] == '*' || c.ptr[0] == '/' || c.ptr[0] == '^'; }
static bool isOp(char c) {
    for (auto o : single_char_ops)
        if (c == o) return true;
    return false;
}
static bool isOp(StringRef token) {
    if (token.len == 1) {
        return isOp(token.ptr[0]);
    } else if (token.len == 2) {
        for (auto o : two_char_ops)
            if (token.eq(o)) return true;
    }
    return false;
}
static Expr *parse_expression(char const *&cursor);
static Expr *parse_inner(char const *&cursor, char a, char b) {
    char const *s = cursor;
    skipSpaces(s);
    if (*s == a) {
        s++;
        Expr *inner = parse_expression(s);
        skipSpaces(s);
        if (*s != b) return NULL;
        s++;
        if (inner == NULL) {
            cursor = s;
            return NULL;
        }
        Expr *expr   = tmp_alloc_expr();
        expr->type   = Type::SCOPE;
        expr->lscope = a;
        expr->rscope = b;
        expr->child  = inner;
        cursor       = s;
        return expr;
    }
    return NULL;
}
static bool parse_symbol(char const *&cursor, StringRef &token) {
    char const *cur = cursor;
    skipSpaces(cur);
    StringRef cur_token{cur, 0};
    while (isLiteral(*cur)) {
        cur++;
        cur_token.len++;
    }
    while (isLiteral(*cur) || isNumeral(*cur)) {
        cur++;
        cur_token.len++;
    }
    if (cur_token.len > 0) {
        cursor = cur;
        token  = cur_token;
        return true;
    }
    return false;
}
static Expr *parse_expression(char const *&cursor) {
    skipSpaces(cursor);
    Expr *lhs = NULL;
    if (*cursor == '(') {
        lhs = parse_inner(cursor, '(', ')');
        if (lhs == NULL) return NULL;
    } else if (isLiteral(*cursor)) {
        StringRef token;
        if (!parse_symbol(cursor, token)) return NULL;
        if (token.eq("defun") || token.eq("technique")) {
            StringRef name = {};
            if (!parse_symbol(cursor, name)) return NULL;
            Expr *argv = parse_inner(cursor, '(', ')');
            // if (argv == NULL) return NULL;
            Expr *body = parse_inner(cursor, '{', '}');
            // if (body == NULL) return NULL;
            Expr *def       = tmp_alloc_expr();
            def->type       = Type::DEFUN;
            def->token      = name;
            def->argv       = argv;
            def->body_scope = body;
            return def;
        }
        skipSpaces(cursor);
        /*if (*cursor == ':') {
           StringRef type;
           if (!parse_symbol(type, type)) return NULL;
           Expr *decl      = tmp_alloc_expr();
           def->type       = Type::DECL;
           def->token      = name;
           def->rhs        = ;
           def->body_scope = body;
           return def;
         }*/
        if (*cursor == '(') {
            Expr *inner = parse_inner(cursor, '(', ')');
            if (inner == NULL) return NULL;
            Expr *call  = tmp_alloc_expr();
            call->type  = Type::CALL;
            call->token = token;
            call->child = inner;
            lhs         = call;
        } else {
            lhs        = tmp_alloc_expr();
            lhs->type  = Type::SYMBOL;
            lhs->token = token;
        }
    } else if (isNumeral(*cursor)) {
        StringRef cur_token{cursor, 1};
        cursor++;
        bool symbol              = false;
        bool has_dot             = false;
        bool only_ones_and_zeros = true;
        while (isLiteral(*cursor) || isNumeral(*cursor) || *cursor == '.') {
            if (*cursor == '.' && symbol) return NULL;
            if (*cursor == '.') has_dot = true;
            if (*cursor != '0' && *cursor != '1') only_ones_and_zeros = false;
            if (isLiteral(*cursor)) {
                symbol = true;
            }
            cursor++;
            cur_token.len++;
        }
        if (symbol) {
            lhs        = tmp_alloc_expr();
            lhs->type  = Type::SYMBOL;
            lhs->token = cur_token;
        } else {
            if (!has_dot && cur_token.len > 1 && cur_token.len <= 4 && only_ones_and_zeros) { // Parse as a swizzle e.g. 001 100 101.
                lhs        = tmp_alloc_expr();
                lhs->type  = Type::SYMBOL;
                lhs->token = cur_token;
            } else {
                double num;
                bool   suc = ParseFloat(cur_token.ptr, (i32)cur_token.len, &num);
                if (!suc) return NULL;
                lhs              = tmp_alloc_expr();
                lhs->type        = Type::VALUE;
                lhs->token       = cur_token;
                lhs->value.type  = ValueType::F64;
                lhs->value.v_f64 = num;
            }
        }
    }
    skipSpaces(cursor);
    if (*cursor == '[') {
        Expr *index_expr = parse_inner(cursor, '[', ']');
        if (index_expr == NULL) return NULL;
        lhs->index_expr = index_expr;
    }
    skipSpaces(cursor);

    if (isOp(*cursor) || isOp({cursor, 2})) {
        Expr *op  = tmp_alloc_expr();
        op->type  = Type::BINOP;
        op->token = StringRef{cursor, 1};
        op->lhs   = lhs;
        if (isOp({cursor, 2})) {
            op->token.len++;
            if (!isOp(op->token)) return NULL;
            cursor += 2;
        } else {
            cursor++;
        }
        Expr *rhs = parse_expression(cursor);
        op->rhs   = rhs;
        if (op->lhs && op->rhs && rhs->type == Type::BINOP) {
            if (getPrecedence(rhs->token) < getPrecedence(op->token)) {
                Expr *a0 = op->lhs;
                Expr *a1 = rhs->lhs;
                Expr *a2 = rhs->rhs;
                assert(a0 && a1 && a0);
                std::swap(op->token, rhs->token);
                rhs->lhs = a0;
                rhs->rhs = a1;
                op->lhs  = rhs;
                op->rhs  = a2;
            }
        }
        return op;
    }
    return lhs;
}
static bool fold(Expr *expr, double &res) {
    if (expr->type == Type::BINOP) {
        double a, b;
        if (expr->lhs && !fold(expr->lhs, a) || !fold(expr->rhs, b)) return false;
        if (expr->token.len == 1) {
            switch (expr->token.ptr[0]) {
            case '+': res = a + b; return true;
            case '-': res = (expr->lhs ? a : 0.0) - b; return true;
            case '*': res = a * b; return true;
            case '/': res = a / b; return true;
            case '^': res = ::pow(a, b); return true;
            default: return false;
            }
        }
        return false;
    } else if (expr->type == Type::VALUE) {
        res = expr->value.v_f64;
        return true;
    } else if (expr->type == Type::SCOPE)
        return fold(expr->child, res);
    return false;
}
static void __test_fold(char const *text, double cmp) {
    TMP_STORAGE_SCOPE;
    char const *cursor = text;
    Expr *      expr   = parse_expression(cursor);
    double      res;
    ASSERT_ALWAYS(fold(expr, res));
    ASSERT_ALWAYS(res == cmp);
}
static void __test_to_string(char const *text, char const *cmp) {
    TMP_STORAGE_SCOPE;
    char const *   cursor = text;
    Expr *         expr   = parse_expression(cursor);
    String_Builder sb;
    sb.Init();
    defer(sb.Release());
    expr->toString(sb);
    ASSERT_ALWAYS(expr && sb.GetStr() == stref_s(cmp));
}
static int __test = [] {
    //__test_fold("(3.2 - 1.2) * 2.0 / 4.0", 1.0);
    //__test_fold("(3.2 - 1.2) * 2.0 / 2.0 ^ 2.0", 1.0);
    //__test_fold("(6.0 - (3.0 * 2.0 - 1.0)) * 2.0 / 2.0", 1.0);
    //__test_fold("3.0 * 2.0 - 1.0", 5.0);
    //__test_fold("-(-3.0 * (-2.0) + 1.0)", -5.0);
    //__test_to_string("a = b; 1 = 2 - 1;", "a=b;1=2-1;");
    //__test_to_string("a : i32 = b; a += c - 1;", "a:i32=b;a+=c-1;");
    //__test_to_string("a : i32 = b; a += c - 1;", "a:i32=b;a+=c-1;");
    //__test_to_string("defun foo (a : float3, b : float3) { dot(a, b) }",
    //"defun foo(a:float3,b:float3){dot(a,b)}");
    return 0;
}();

}; // namespace TopGSL

class GfxEvaluator {

private:
    struct Value;
    struct TextureInfo {
        std::shared_ptr<Value> width  = {};
        std::shared_ptr<Value> height = {};
        std::shared_ptr<Value> format = {};
        std::shared_ptr<Value> init   = {};
    };
    struct BufferInfo {
        std::shared_ptr<Value> num_elements = {};
        std::shared_ptr<Value> format       = {};
        std::shared_ptr<Value> init         = {};
    };
    struct Value {
        enum class Value_t : i32 {
            UNKNOWN = 0,  //
            I32,          //
            F32,          //
            SYMBOL,       //
            REFERENCE,    //
            TEXTURE_INFO, //
            TEXTURE,      //
            BUFFER_INFO,  //
            BUFFER,       //
            ANY
        };
        Value_t type     = Value_t::UNKNOWN;
        i32     any_type = i32(-1);
        union {
            StringRef     str;
            f32           f;
            i32           i;
            sexpr::SNode *node;

        } v                      = {};
        GfxContext  gfx          = {};
        TextureInfo texture_info = {};
        GfxTexture  texture      = {};
        BufferInfo  buffer_info  = {};
        GfxTexture  buffer       = {};

        Value() = default;
        ~Value() {
            if (texture) gfxDestroyTexture(gfx, texture);
            if (buffer) gfxDestroyTexture(gfx, buffer);
        }
        static std::shared_ptr<Value> CreateRef(GfxContext _gfx, sexpr::SNode *node) {
            Value *v  = new Value;
            v->gfx    = _gfx;
            v->type   = Value_t::REFERENCE;
            v->v.node = node;
            return std::shared_ptr<Value>(v);
        }
        static std::shared_ptr<Value> CreateI32(GfxContext _gfx, sexpr::SNode *node) {
            Value *v = new Value;
            v->gfx   = _gfx;
            v->type  = Value_t::I32;
            v->v.i   = node->ParseInt();
            return std::shared_ptr<Value>(v);
        }
        static std::shared_ptr<Value> CreateF32(GfxContext _gfx, sexpr::SNode *node) {
            Value *v = new Value;
            v->gfx   = _gfx;
            v->type  = Value_t::F32;
            v->v.i   = node->ParseFloat();
            return std::shared_ptr<Value>(v);
        }
        static std::shared_ptr<Value> CreateTextureInfo(GfxContext _gfx, TextureInfo const &_texture_info) {
            Value *v        = new Value;
            v->gfx          = _gfx;
            v->type         = Value_t::TEXTURE_INFO;
            v->texture_info = _texture_info;
            return std::shared_ptr<Value>(v);
        }
        static std::shared_ptr<Value> CreateTexture(GfxContext _gfx, GfxTexture _texture) {
            Value *v   = new Value;
            v->gfx     = _gfx;
            v->type    = Value_t::TEXTURE;
            v->texture = _texture;
            return std::shared_ptr<Value>(v);
        }
    };

    struct Symbol_Frame {
        using Table_t       = std::unordered_map<StringRef, std::shared_ptr<Value>>;
        Table_t       table = {};
        Symbol_Frame *prev  = {};
        void          Init() {
            table = {};
            prev  = NULL;
        }
        void Release() {
            table.clear();
            prev = NULL;
            delete this;
        }
        static Symbol_Frame *Create() {
            Symbol_Frame *o = new Symbol_Frame;
            o->Init();
            return o;
        }
        std::shared_ptr<Value> Get(StringRef name) {
            auto it = table.find(name);
            if (it != table.end()) return it->second;
            return NULL;
        }
        void Insert(StringRef name, std::shared_ptr<Value> val) {
            if (std::shared_ptr<Value> old_val = Get(name)) {
                old_val.reset();
            }
            table[name] = val;
        }
    };

private:
    GfxContext                                                                           gfx   = {};
    sexpr::SNode *                                                                       root  = NULL;
    std::unordered_map<StringRef, std::function<std::shared_ptr<Value>(sexpr::SNode *)>> funcs = {};

    std::vector<Symbol_Frame *> table_storage    = {};
    Symbol_Frame *              tail             = {};
    Symbol_Frame *              head             = {};
    BlueNoiseBaker              blue_noise_baker = {};

    std::vector<StringRef> FindTokens(StringRef text) {
        std::vector<StringRef> result = {};
        if (text.len == u64(0)) return {};
        char const *cursor      = text.ptr;
        u64         len         = u64(0);
        auto        flush_token = [&] {
            if (len > u64(0)) result.push_back(StringRef(cursor, len));
        };
        ifor(text.len) {
            // len++;
            char c = cursor[len];
            switch (c) {
            case '.':
            case ';':
            case ',':
            case ' ':
            case '\n':
            case '\r':
            case '*':
            case '+':
            case '-':
            case '/':
            case '^':
            case '@':
            case '$':
            case '&':
            case '!':
            case '#':
            case '%':
            case '`':
            case '\'':
            case '{':
            case '}':
            case '[':
            case ']':
            case '(':
            case ')':
            case '\"': {
                flush_token();
                // result.push_back(StringRef(cursor + len, u64(1)));
                cursor += len + u64(1);
                len = u64(0);
            } break;
            default: len++;
            }
        }
        flush_token();
        return result;
    }

    struct BakedKernel {
        bool  blue_noise_used;
        char *text; // on thread local temporary storage
    };

    void LaunchKernel(                                                             //
        StringRef                                                   code,          //
        u32x3                                                       dispatch_size, //
        u32x3                                                       group_size,    //
        std::vector<std::pair<std::string, std::shared_ptr<Value>>> bindings       //
        //
    ) {
        TMP_STORAGE_SCOPE;

        struct StringBuilder {
            char *begin  = NULL;
            u64   size   = u64(0);
            u64   cursor = u64(0);

            void Init(char *_begin, u64 _size) {
                begin         = _begin;
                size          = _size;
                begin[cursor] = '\0';
            }
            void Append(StringRef text) {
                u64 i = u64(0);
                while (text.ptr[i] != '\0' && i < text.len) {
                    begin[cursor] = text.ptr[i];
                    cursor++;
                    i++;
                    assert(cursor < size);
                }
                begin[cursor] = '\0';
            }
            void Append(char const *str) {
                while (str[0] != '\0') {
                    begin[cursor] = str[0];
                    cursor++;
                    str++;
                    assert(cursor < size);
                }
                begin[cursor] = '\0';
            }
            void Finish() {
                begin[cursor] = '\0';
                cursor++;
                assert(cursor < size);
            }
        };
        StringBuilder binding_builder = {};

        u64 bindings_buffer_size   = u64(1 << 20);
        u64 final_text_buffer_size = u64(1 << 20);
        u64 main_buffer_size       = u64(1 << 20);
        u64 group_size_buffer_size = u64(128);

        assert(code);

        std::vector<StringRef> tokens = FindTokens(code);

        BakedKernel baked_kernel = {};

        std::unordered_map<StringRef, std::function<void()>> builtin_map = {
            //
            {StringRef("__builtin_blue_noise"), [&] { baked_kernel.blue_noise_used = true; }}, //
        };

        for (auto &t : tokens) {
            auto it = builtin_map.find(t);
            if (it != builtin_map.end()) {
                it->second();
            }
        }

        char *bindings_buffer = tl_alloc_tmp<char>(bindings_buffer_size);
        binding_builder.Init(bindings_buffer, bindings_buffer_size);
        char *group_size_buffer = tl_alloc_tmp<char>(group_size_buffer_size);

        baked_kernel.text = tl_alloc_tmp<char>(final_text_buffer_size);
        char *main_code   = tl_alloc_tmp<char>(main_buffer_size);

        snprintf(group_size_buffer, group_size_buffer_size, "[numthreads(%i, %i, %i)]", group_size.x, group_size.y, group_size.z);
        for (auto &i : bindings) {
            if (i.second->type == Value::Value_t::TEXTURE) {
                char const *texture_format = "f32x4";
                if (i.second->texture.getFormat() == DXGI_FORMAT_R32G32B32A32_FLOAT) {

                } else {
                    UNIMPLEMENTED;
                }
                char tmp_bind_line[0x100];
                sprintf_s(tmp_bind_line, "RWTexture2D<%s> %s;\n", texture_format, i.first.c_str());
                binding_builder.Append(tmp_bind_line);
            } else if (i.second->type == Value::Value_t::BUFFER) {
                UNIMPLEMENTED;
            } else {
                UNIMPLEMENTED;
            }
        }

        StringBuilder main_builder = {};
        main_builder.Init(main_code, main_buffer_size);

        if (baked_kernel.blue_noise_used) {
            binding_builder.Append("Texture2D<f32x2> __builtin_blue_noise_texture;");
            main_builder.Append("f32x2 __builtin_blue_noise = __builtin_blue_noise_texture[tid.xy & u32(0x7f)];\n");
        }

        main_builder.Append(code);

        snprintf(baked_kernel.text, final_text_buffer_size, R"(
#include "common.h"

%s

%s
void main(u32x3 tid : SV_DispatchThreadID, u32x3 gid : SV_GroupThreadID) {
%s;
}
)",
                 bindings_buffer, group_size_buffer, main_code);

        GfxProgram program = gfxCreateProgram(gfx, GfxProgramDesc::Compute(baked_kernel.text));
        assert(program);
        GfxKernel kernel = gfxCreateComputeKernel(gfx, program, "main");
        assert(kernel);
        for (auto &i : bindings) {
            if (i.second->type == Value::Value_t::TEXTURE) {
                gfxProgramSetTexture(gfx, program, i.first.c_str(), i.second->texture);
            } else if (i.second->type == Value::Value_t::BUFFER) {
                UNIMPLEMENTED;
            } else {
                UNIMPLEMENTED;
            }
        }
        if (baked_kernel.blue_noise_used) {
            gfxProgramSetTexture(gfx, program, "__builtin_blue_noise_texture", blue_noise_baker.GetTexture());
        }
        gfxCommandBindKernel(gfx, kernel);
        gfxCommandDispatch(gfx, (dispatch_size.x + group_size.x - u32(1)) / group_size.x, //
                           (dispatch_size.y + group_size.y - u32(1)) / group_size.y,      //
                           (dispatch_size.z + group_size.z - u32(1)) / group_size.z);
    }

    std::shared_ptr<Value> Eval(sexpr::SNode *node) {
        while (node) {
            if (node->symbol) {
                auto it = funcs.find(node->symbol);
                if (it != funcs.end()) {
                    return it->second(node);
                } else if (node->symbol.eq("let")) {
                    assert(node->next && node->next->symbol && node->next->next);
                    assert(node->next->next->next == NULL && "let $name $value nil");
                    std::shared_ptr<Value> v = {};
                    if (node->squoted)
                        v = Value::CreateRef(gfx, node->next->next);
                    else
                        v = Eval(node->next->next);
                    AddSymbol(node->next->symbol, v);
                    return v;
                } else {
                    if (std::shared_ptr<Value> v = LookupValue(node->symbol)) {
                        while (v->type == Value::Value_t::REFERENCE) {
                            std::shared_ptr<Value> new_val = Eval(v->v.node);
                            if (new_val->type == Value::Value_t::REFERENCE && new_val->v.node == v->v.node) return v; // Evaluates to self
                            v = new_val;
                        }
                        return v;
                    }
                    if (node->IsInt()) {
                        return Value::CreateI32(gfx, node);
                    }
                    if (node->IsF32()) {
                        return Value::CreateF32(gfx, node);
                    }
                    return Value::CreateRef(gfx, node);
                }
                UNIMPLEMENTED;
            } else if (node->child) {
                EnterScope();
                std::shared_ptr<Value> e = Eval(node->child);
                ExitScope();
                if (node->next == NULL) return e;
            }
            node = node->next;
        }
        UNIMPLEMENTED;
    }
    u32 EvalToU32(std::shared_ptr<Value> val) {
        while (val->type == Value::Value_t::REFERENCE) {
            val = Eval(val->v.node);
        }
        assert(val->type == Value::Value_t::I32);
        return val->v.i;
    }
    DXGI_FORMAT EvalToFormat(std::shared_ptr<Value> val) {
        while (val->type == Value::Value_t::REFERENCE) {
            if (val->v.node->symbol.eq("R32G32B32A32_FLOAT")) {
                return DXGI_FORMAT_R32G32B32A32_FLOAT;
            }
            val = Eval(val->v.node);
        }
        assert(val->type == Value::Value_t::SYMBOL);
        if (val->v.str.eq("R32G32B32A32_FLOAT")) {
            return DXGI_FORMAT_R32G32B32A32_FLOAT;
        }
        UNIMPLEMENTED;
    }
    void Init(GfxContext _gfx, sexpr::SNode *_root) {
        gfx  = _gfx;
        root = _root;

        blue_noise_baker.Init(gfx);
        blue_noise_baker.Bake();

        {
            GfxTexture texture     = blue_noise_baker.GetTexture();
            GfxBuffer  dump_buffer = write_texture_to_buffer(gfx, texture);
            WaitIdle(gfx);
            f32x4 *host_rgba_f32x4 = gfxBufferGetData<f32x4>(gfx, dump_buffer);
            write_f32x4_png("blue_noise.png", host_rgba_f32x4, texture.getWidth(), texture.getHeight());
            gfxDestroyBuffer(gfx, dump_buffer);
        }

        table_storage.push_back(Symbol_Frame::Create());
        tail = table_storage[0];
        head = table_storage[0];

        funcs["@print"] = [&](sexpr::SNode *node) -> std::shared_ptr<Value> {
            assert(node->next);
            node = node->next;
            assert(node->symbol);
            std::shared_ptr<Value> val = LookupValue(node->symbol);
            assert(val);
            // val->Dump();
            return val;
        };
        funcs["@eval"] = [&](sexpr::SNode *node) -> std::shared_ptr<Value> {
            assert(node->next);
            node = node->next;
            return Eval(node);
        };
        funcs["@make_texture"] = [&](sexpr::SNode *node) -> std::shared_ptr<Value> {
            assert(node->next);
            node             = node->next;
            TextureInfo info = {};
            while (node) {
                if (node->child) {
                    sexpr::SNode *val = node->child;
                    assert(val->symbol);
                    assert(val->next);
                    assert(val->next->next == NULL);

                    if (val->symbol.eq(".width")) {
                        info.width = Eval(val->next);
                    } else if (val->symbol.eq(".height")) {
                        info.height = Eval(val->next);
                    } else if (val->symbol.eq(".format")) {
                        info.format = Eval(val->next);
                    } else if (val->symbol.eq(".init")) {
                        info.init = Eval(val->next);
                    } else {
                        UNIMPLEMENTED;
                    }
                }
                node = node->next;
            }
            return Value::CreateTextureInfo(gfx, info);
        };
        funcs["@materialize"] = [&](sexpr::SNode *node) -> std::shared_ptr<Value> {
            assert(node->next);
            node                                    = node->next;
            std::shared_ptr<Value> texture_info_val = Eval(node);
            if (texture_info_val->type == Value::Value_t::TEXTURE_INFO) {
                assert(texture_info_val->type == Value::Value_t::TEXTURE_INFO);
                u32         width   = EvalToU32(texture_info_val->texture_info.width);
                u32         height  = EvalToU32(texture_info_val->texture_info.height);
                DXGI_FORMAT format  = EvalToFormat(texture_info_val->texture_info.format);
                GfxTexture  texture = gfxCreateTexture2D(gfx, width, height, format);
                if (texture_info_val->texture_info.init) {
                    assert(texture_info_val->texture_info.init->type == Value::Value_t::REFERENCE);
                    assert(texture_info_val->texture_info.init->v.node->quoted == true);
                    TMP_STORAGE_SCOPE;
                    char *tmp = tl_alloc_tmp<char>(u64(1 << 20));
                    snprintf(tmp, u64(1 << 20), R"(
#include "common.h"
RWTexture2D<f32x4> g_target;
[numthreads(8, 8, 1)]
void main(u32x3 tid : SV_DispatchThreadID) {
    g_target[tid.xy] = %.*s;
}
)",
                             STRF(texture_info_val->texture_info.init->v.node->symbol));
                    GfxProgram program = gfxCreateProgram(gfx, GfxProgramDesc::Compute(tmp));
                    assert(program);
                    GfxKernel kernel = gfxCreateComputeKernel(gfx, program, "main");
                    assert(kernel);
                    gfxProgramSetTexture(gfx, program, "g_target", texture);
                    gfxCommandBindKernel(gfx, kernel);
                    gfxCommandDispatch(gfx, (width + u32(7)) / u32(8), (height + u32(7)) / u32(8), u32(1));
                    gfxDestroyKernel(gfx, kernel);
                    gfxDestroyProgram(gfx, program);
                }
                return Value::CreateTexture(gfx, texture);
            } else {
                UNIMPLEMENTED;
            }
        };
        funcs["@write_to_file"] = [&](sexpr::SNode *node) -> std::shared_ptr<Value> {
            assert(node->next);
            node = node->next;
            assert(node->next);
            std::shared_ptr<Value> val = Eval(node);
            if (val->type == Value::Value_t::TEXTURE) {
                assert(val->type == Value::Value_t::TEXTURE);

                GfxBuffer dump_buffer = write_texture_to_buffer(gfx, val->texture);
                WaitIdle(gfx);

                f32x4 *host_rgba_f32x4 = gfxBufferGetData<f32x4>(gfx, dump_buffer);

                char tmp[0x100];
                snprintf(tmp, sizeof(tmp), "%.*s", STRF(node->next->symbol));

                write_f32x4_png(tmp, host_rgba_f32x4, val->texture.getWidth(), val->texture.getHeight());

                gfxDestroyBuffer(gfx, dump_buffer);

                return NULL;
            } else {
                UNIMPLEMENTED;
            }
        };
        funcs["@dispatch"] = [&](sexpr::SNode *node) -> std::shared_ptr<Value> {
            assert(node->next);
            node = node->next;
            struct DispatchInfo {
                StringRef                                                   code          = {};
                u32x3                                                       dispatch_size = {1, 1, 1};
                u32x3                                                       group_size    = {8, 8, 1};
                std::vector<std::pair<std::string, std::shared_ptr<Value>>> bindings      = {};
            } info = {};
            while (node) {
                if (node->child) {
                    if (node->child->symbol.eq(".dispatch_size")) {
                        assert(node->child->next);
                        assert(node->child->next->next);
                        assert(node->child->next->next->next);

                        auto x = EvalToU32(Eval(node->child->next));
                        auto y = EvalToU32(Eval(node->child->next->next));
                        auto z = EvalToU32(Eval(node->child->next->next->next));

                        info.dispatch_size.x = x;
                        info.dispatch_size.y = y;
                        info.dispatch_size.z = z;

                    } else if (node->child->symbol.eq(".group_size")) {
                        assert(node->child->next);
                        assert(node->child->next->next);
                        assert(node->child->next->next->next);

                        auto x = EvalToU32(Eval(node->child->next));
                        auto y = EvalToU32(Eval(node->child->next->next));
                        auto z = EvalToU32(Eval(node->child->next->next->next));

                        info.group_size.x = x;
                        info.group_size.y = y;
                        info.group_size.z = z;

                        assert(info.group_size.x && info.group_size.y && info.group_size.z);
                        assert((info.group_size.x * info.group_size.y * info.group_size.z % u32(32)) == u32(0));
                    } else if (node->child->symbol.eq(".bind")) {
                        assert(node->child->next);
                        std::shared_ptr<Value> v = Eval(node->child->next);
                        assert(v && v->type == Value::Value_t::TEXTURE || v->type == Value::Value_t::BUFFER);
                        info.bindings.push_back({node->child->next->symbol.to_str(), v});
                    } else if (node->child->symbol.eq(".code")) {
                        assert(node->child->next);
                        info.code = node->child->next->symbol;
                    } else {

                        UNIMPLEMENTED;
                    }
                }
                node = node->next;
            }
            assert(info.dispatch_size.x && info.dispatch_size.y && info.dispatch_size.z);
            assert(info.group_size.x && info.group_size.y && info.group_size.z);
            assert((info.group_size.x * info.group_size.y * info.group_size.z % u32(32)) == u32(0));

            LaunchKernel(info.code, info.dispatch_size, info.group_size, info.bindings);

            return NULL;
        };
    }
    void EnterScope() {
        Symbol_Frame *new_table = Symbol_Frame::Create();
        table_storage.push_back(new_table);
        new_table->prev = tail;
        tail            = new_table;
    }
    void ExitScope() {
        Symbol_Frame *new_tail = tail->prev;
        assert(new_tail != NULL);
        tail->Release();
        table_storage.pop_back();
        tail = new_tail;
    }
    std::shared_ptr<Value> LookupValue(StringRef name) {
        Symbol_Frame *cur = tail;
        while (cur != NULL) {
            if (std::shared_ptr<Value> val = cur->Get(name)) return val;
            cur = cur->prev;
        }
        return NULL;
    }
    void AddSymbol(StringRef name, std::shared_ptr<Value> val) {
        assert(tail && tail->prev); // Adding to the parent scope with (let a b)
        tail->prev->Insert(name, val);
    }

public:
    static GfxEvaluator *Create(GfxContext _gfx, sexpr::SNode *_root) {
        GfxEvaluator *o = new GfxEvaluator;
        o->Init(_gfx, _root);
        return o;
    }
    void Eval() { Eval(root); }
    void Release(GfxContext _gfx) {
        for (auto t : table_storage) {
            t->Release();
        }
        table_storage.clear();
        delete this;
    }
};