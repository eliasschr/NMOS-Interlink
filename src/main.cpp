#include <iostream>
#include <string_view>

namespace {

constexpr std::string_view kProgramName{"nmos-interlink"};
constexpr std::string_view kVersion{NMOS_INTERLINK_VERSION};

void print_help()
{
    std::cout << "Usage: " << kProgramName << " [--help|-h] [--version|-V]\n"
              << "\n"
              << "NMOS Interlink bootstrap command-line interface.\n"
              << "\n"
              << "Options:\n"
              << "  -h, --help     Show this help message.\n"
              << "  -V, --version  Show the program version.\n";
}

void print_version()
{
    std::cout << kProgramName << " " << kVersion << "\n";
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc == 1) {
        print_help();
        return 0;
    }

    const std::string_view argument{argv[1]};
    if (argc > 2) {
        std::cerr << kProgramName << ": unexpected argument: " << argv[2] << "\n";
        return 2;
    }

    if (argument == "--help" || argument == "-h") {
        print_help();
        return 0;
    }

    if (argument == "--version" || argument == "-V") {
        print_version();
        return 0;
    }

    std::cerr << kProgramName << ": unknown option: " << argument << "\n";
    return 2;
}
