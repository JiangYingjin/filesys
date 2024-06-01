#include <iostream>
#include <sstream>
using namespace std;

#define PRODUCTION 0

class InfoLogger
{
public:
    template <typename T>
    InfoLogger &operator<<(const T &value)
    {
        cout << "[INFO] " << value;
        return *this;
    }

    ~InfoLogger()
    {
        if (!PRODUCTION)
            cout << endl;
    }
};

InfoLogger info();
