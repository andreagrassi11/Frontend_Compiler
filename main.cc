#include <iostream>

extern "C" {
    double g(double, double);
}

int main() {
    int x,y;
    std::cout << "Inserisci due numeri interi, x e y\nx: ";
    std::cin >> x;
    std::cout << "y: ";
    std::cin >> y;
    std::cout << "Il valore di g(x,y) è " << g(x,y) << std::endl;
}
