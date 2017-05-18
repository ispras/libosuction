#include <iostream>
#include <memory>
#include "animal.hpp"

using std::cout;
using std::endl;
using std::unique_ptr;

class Dog : public Animal {
public:
  Dog() {
    cout << "Dog was born." << endl;
  }
  ~Dog() {
    cout << "Dog died." << endl;
  }
  virtual void Cry() const override {
     cout << "\"Bow-wow\"" << endl;
  }
};

extern "C" unique_ptr<Animal> Create() {
  return unique_ptr<Animal>(new Dog);
}
