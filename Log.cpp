#include <iostream>
#include <string>
using namespace std;

#define PRODUCTION 0

class InfoLogger
{
public:
    template <typename T>
    friend InfoLogger &operator<<(InfoLogger &logger, const char *data)
    {
        if (!PRODUCTION)
            cout << data;
        // cout << "[INFO] " << data;
        return logger;
    }

    // friend InfoLogger &operator<<(ostream &(*manip)(ostream &))
    // {
    //     if (!PRODUCTION)
    //         cout << manip;
    //     return *this;
    // }
};

InfoLogger info();
