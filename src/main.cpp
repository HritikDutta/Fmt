#include "core/logger.h"
#include "containers/string.h"
#include "fileio/fileio.h"
#include "serialization/slz.h"
#include "serialization/yaml.h"

#include "formatter/fmt_lexer.h"
#include "formatter/fmt_parser.h"

int main(int argc, char** argv)
{
    #ifndef GN_DEBUG
    if (argc < 2)
    {
        print_error("Need a json/yaml file to format");
        return 1;
    }

    String content = file_load_string(ref(argv[1]));

    if (!content.data)
        return 1;
    #else
    String content = file_load_string(ref("tests/template.yaml"));
    #endif

    Slz::Document document = {};
    
    // Ideally I should check the extension or something to select yaml or json parser.
    // But the yaml parser can parse any valid json string so this should work fine.
    if (!Yaml::parse_string(content, document))
    {
        print_error("Couldn't parse template json!");
        return 1;
    }

    Slz::Object start = document.start().object();
    Slz::Array templates = start[ref("templates")].array();

    Fmt::Pass pass = {};
    Fmt::prepare_data(pass, document.start());

    StringBuilder builder = {};
    StringBuilder file_path_builder = {};

    Fmt::Pass out_file_pass = {};

    for (int i = 0; i < templates.size(); i++) 
    {
        Slz::Object template_data = templates[i].object();
        const String template_content = template_data[ref("template")].string();
        const String file_path_content = template_data[ref("output")].string();

        if (!Fmt::tokenize(template_content, pass.tokens))
        {
            print_error("Error tokenizing template!\n");
            continue; // Move on to next template
        }

        if (!Fmt::tokenize(file_path_content, out_file_pass.tokens))
        {
            print_error("Error tokenizing output file path!\n");
            continue; // Move on to next template
        }

        Slz::Array passes = template_data[ref("passes")].array();
        for (int pass_idx = 0; pass_idx < passes.size(); pass_idx++)
        {
            Fmt::prepare_pass(pass, passes[pass_idx]);
            if (!Fmt::parse_template(template_content, pass, builder))
            {
                print_error("Error parsing template!\r\n");
                break;  // No point trying the other passes I guess
            }

            out_file_pass.root_var = pass.root_var;
            if (!Fmt::parse_template(file_path_content, out_file_pass, file_path_builder))
            {
                print_error("Error parsing output file path!\r\n");
                break;  // No point trying the other passes I guess
            }

            String template_output  = build_string(builder);
            String file_path_output = build_string(file_path_builder);

            file_write_string(file_path_output, template_output);
            print("Written to: '%'", file_path_output);

            free(template_output);
            free(file_path_output);
        }
    }

    // Warning there'll be memory leaks.

    free(document);
    free(content);
}
