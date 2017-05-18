#ifndef ANIMAL_HPP
#define ANIMAL_HPP

#include <memory>

using std::unique_ptr;

class Animal {
public:
  virtual ~Animal() {};
  virtual void Cry() const = 0;
};

typedef unique_ptr<Animal> AnimalCreateFunc();

#endif // ANIMAL_HPP
