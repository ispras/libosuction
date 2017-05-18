#include <iostream>
#include <memory>
#include "animal.hpp"

using std::cout;
using std::endl;
using std::unique_ptr;

class Cat : public Animal {
public:
  Cat() {
    cout << "Cat was born." << endl;
  }
  ~Cat() {
    cout << "Cat died." << endl;
  }
  void Cry() const override {
     cout << "\"Meow\"" << endl;
  }
};

extern "C" unique_ptr<Animal> Create() {
  return unique_ptr<Animal>(new Cat);
}
