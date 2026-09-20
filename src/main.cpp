#include "Application.h"

#include <iostream>
#include <string>

int main(int argc, char* argv[])
{
    Application app;

    if (argc < 2)
    {
        return app.RunAll();
    }

    std::string cmd = argv[1];

    if (cmd == "--price")
    {
        return app.RunPrice();
    }

    if (cmd == "--fundamental")
    {
        return app.RunFundamental();
    }

    if (cmd == "--all")
    {
        return app.RunAll();
    }

    std::cout
        << "Usage:\n"
        << "  FundamentalUpdater_rev6\n"
        << "  FundamentalUpdater_rev6 --price\n"
        << "  FundamentalUpdater_rev6 --fundamental\n"
        << "  FundamentalUpdater_rev6 --all\n";

    return 0;
}
