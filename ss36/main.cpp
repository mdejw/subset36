/**
* This file is part of "balise_codec".
* balise_codec is free software: you can distribute it and/or modify it under the terms of the GNU Lesser General Public License as
* published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
*
* See the GNU Lesser General Public License for more details.
* You should have received a copy of the GNU General Public License along with this program.
* If not, see < https://www.gnu.org/licenses/>.
*/

#include "balise_codec.h"
#include "CLI11.hpp"            // parse command line parameters

int verbose = VERB_PROG;

int main(int argc, char** argv)
{
    t_telegramlist telegrams;           // list of telegrams
    int result = ERR_NO_ERR;            // end result of this program (0=success)
    clock_t start, end;
    double execution_time;
    string output_text;                 // the output of the program

    // command line parameters:
    string input_file = "";             // the input file name
    string output_file = "";            // the output file name
    string literal = "";                // the input literal 
    string output_format = "";          // the output format (if set to hex, output as hex. Otherwise: output in base64)
    int max_cpu = 0;                    // maximum nr of simultaneous processes (0=unlimited)
    bool force_long = false;            // force long format for shaped telegrams
    bool show_err = false;              // show the meaning of the error codes
    bool error_only = false;            // only show output lines that contain an error

    setupConsole();  // for colorful output

    // setup and parse the command line parameters:
    CLI::App app{ "This is BALISE_CODEC by fokke@bronsema.net. See https://github.com/FokkeB/subset36 for more details. Version: " PROG_VERSION };
    app.add_option("-i,--input_filename", input_file, "Read lines with data from the indicated csv-file (UTF - 8, no BOM) and convert its contents from shaped data to unshaped data and vice versa. This tool automatically determines the used format (base64 / hex) and length (short / long). Lines must be separated by '\\n' ('\\r' will be ignored). If both the shaped and unshaped data are given on one line (separated by a comma or semicolon), this tool will check the correct shaping. Comments must be preceded by '#'.");
    app.add_option("-o,--output_filename", output_file, "Write output to this file.");
    app.add_option("-s,--string", literal, "Input string (shaped and/or deshaped string in base64/hex), format identical to one line in the input file.");
    app.add_option("-v,--verbose", verbose, "Level of verbosity: 0 (quiet, only show result), 1 (+show progress, default), 2 (+basic output) or 3 (+lots of output).");
    app.add_option("-m,--max_cpu", max_cpu, "Max nr of cpu's to use. Multithreading is enabled by default for verbosity <= 1 or if set to 0.");
    app.add_flag("-l,--force_long", force_long, "Force shaping to the long format (1023 bits), even if the unshaped data is of short format (341 bits). This only works on telegrams in which the unshaped user data is set and the shaped data is not set. If not specified, this tool will use the same format as the input data.");
    app.add_option("-f,--format_output", output_format, "Output format for the shaped telegram: 'hex' or 'base64' (default).");
    app.add_flag("-e,--show_error_codes", show_err, "Shows the meaning of the error codes that can be generated when checking / shaping telegrams.");
    app.add_flag("-E,--error_only", error_only, "Output only the telegrams in which an error was found (-e gives the error codes).");
    CLI11_PARSE(app, argc, argv);

    if (show_err)
        // show the meaning of the error messages and die
    {
        printf("The error codes below can be generated when checking / shaping a telegram. \n");
        printf("See SUBSET - 036 paragraph 4.3 for more details concerning error codes >= 10.\n");
        printf("Increase verbosity to 2 to see detailed information about the errors found during conversion.\n");
        printf("Error code\tExplanation\n");
        printf("\t0\tNo error\n");
        printf("\t1\tNo input specified\n");
        printf("\t2\tA logical error (not further specified)\n");
        printf("\t3\tError creating output file\n");
        printf("\t4\tError during memory allocation\n");
        printf("\t10\tAlphabet condition fails\n");
        printf("\t11\tOff-sync parsing condition fails\n");
        printf("\t12\tAperiodicity condition fails\n");
        printf("\t13\tUndersampling check fails\n");
        printf("\t14\tControl bits check fails\n");
        printf("\t15\tCheck bits check fails\n");
        printf("\t16\tOverflow of SB and ESB (should never occur, please contact author if it did)\n");
        printf("\t17\tError during conversion from 10 bits to 11 bits (11-bit value not found in list of transformation words)\n");
        printf("\t18\tShaped contents do not match the unshaped contents (encoding error)\n");

        return ERR_NO_ERR;
    }

    // execute the commands from the command line:

    // first get the input (either input file or literal)
    if (input_file != "")
    {
        // filename is set, load the data from the file:
        //telegrams = load_telegrams_from_file(input_file);

        telegrams = parse_content_string(read_from_file(input_file));
    }
    else
    {
        telegrams.push_back(parse_input_line(literal.c_str()));
        if (telegrams.front() == NULL)
        {
            eprintf(VERB_QUIET, ERROR_COLOR "ERROR: No input specified, quitting.\n" ANSI_COLOR_RESET);
            exit(ERR_NO_INPUT);
        }
    }

    // set the force_long parameter of the telegrams:
    if (force_long)
        for (telegram*& tel : telegrams)
            tel->force_long = true;

    // set the #processes to 1 if the verbosity level is higher than VERB_PROG
    if (verbose > VERB_PROG)
        max_cpu = 1;

    start = clock();

    // convert the input to the other format or check the correctness of a telegram:
    result = convert_telegrams_multithreaded(telegrams, max_cpu);

    end = clock();
    execution_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    eprintf(VERB_PROG, "Calculation time: %.2f secs\n", execution_time);

    // determine the output string:
    output_text = output_telegrams_to_string(telegrams, output_format, error_only, true);

    // output the result to the indicated medium:
    if (output_file != "")
        output_telegrams_to_file(output_text, output_file);
    else
        eprintf(VERB_QUIET, "%s", output_text.c_str());

//    eprintf(VERB_QUIET, "Ready. Press any key to continue ...");
//    char dummy = getch();

    restoreConsole();

    return get_first_error_code (telegrams);
}