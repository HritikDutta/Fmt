#include "core/logger.h"
#include "containers/string.h"
#include "fileio/fileio.h"
#include "serialization/json.h"

#include "formatter/fmt_lexer.h"
#include "formatter/fmt_parser.h"

int main(int argc, char** argv)
{
    #ifndef GN_DEBUG
    if (argc < 2)
    {
        print_error("Need a json file to format");
        return 1;
    }

    String content = file_load_string(ref(argv[1]));
    #else
    String content = file_load_string(ref("tests/test_template.json"));
    #endif

    Json::Document document = {};
    
    if (!Json::parse_string(content, document))
    {
        print_error("Couldn't parse template json!");
        return 1;
    }

    Json::Object start = document.start().object();
    Json::Array templates = start[ref("templates")].array();

    Fmt::Pass pass = {};
    Fmt::prepare_data(pass, document.start());

    StringBuilder builder = {};

    for (int i = 0; i < templates.size(); i++) 
    {
        Json::Object template_data = templates[i].object();
        const String template_content = template_data[ref("template")].string();

        if (!Fmt::tokenize(template_content, pass.tokens))
        {
            print_error("Error tokenizing template!\n");
            continue; // Move on to next template
        }

        Json::Array passes = template_data[ref("passes")].array();
        for (int pass_idx = 0; pass_idx < passes.size(); pass_idx++)
        {
            Fmt::prepare_pass(pass, passes[pass_idx]);
            if (!Fmt::parse_template(template_content, pass, builder))
            {
                print_error("Error parsing template!\n");
                break;  // No point trying the other passes I guess
            }

            String template_content = build_string(builder);
            print("%\n", template_content);
            free(template_content);
        }
    }

    // Warning there'll be memory leaks.

    free(document);
    free(content);
}
