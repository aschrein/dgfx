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

#if !defined(XML_CONFIG_HPP)
#    define XML_CONFIG_HPP

#    define XML_READ_F32(x)                                                                                                                                                        \
        if (strcmp(attr.name(), #x) == i32(0)) {                                                                                                                                   \
            x = attr.as_float();                                                                                                                                                   \
        }

#    define XML_WRITE_F32(x) fprintf(file, #    x "=\"%f\" ", x);

struct Value {
    std::string name;
    f32         f32_value;
};
struct XMLConfig {
    void Restore(std::function<void(pugi::xml_node)> child_callback) {
        pugi::xml_document doc         = {};
        pugi::xml_node     config_node = {};
        std::string        state       = read_file("config.xml");
        if (state.size()) {
            pugi::xml_document     doc    = {};
            pugi::xml_parse_result result = doc.load_buffer(state.c_str(), state.size());
            if (result) {
                if (config_node = doc.child("config")) {
                    for (auto &c : config_node.children()) {
                        child_callback(c);
                    }
                }
            }
        }
    }
    void Store(std::function<void(FILE *)> callback) {
        FILE *config_file = NULL;
        int   err         = fopen_s(&config_file, "config.xml", "wb");
        if (err) return;
        fprintf(config_file, "<config>\n");
        callback(config_file);
        fprintf(config_file, "</config>\n");
        fflush(config_file);
        fclose(config_file);
    }
};

#endif // XML_CONFIG_HPP