#include <iostream>

extern "C" {
    double g(double, double);
}

int main() {
    int x,y;
    std::cout << "Inserisci il valore di x: ";
    std::cin >> x;
    std::cout << "Inserisci il valore di y: ";
    std::cin >> y;

    std::cout << "Il valore di g(x,y) è " << g(x, y) << std::endl;
}
