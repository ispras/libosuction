struct Obj
{
  Obj(const char *d) {
    this->d = d;
  }
  const char *d;
};

struct MakefileGenerator
{
  virtual Obj foo() const;
};

#define QStringLiteral(str) \
    ([]() -> Obj { \
        static const char *data = str; \
        Obj obj = { data }; \
        return obj; \
    }())

inline Obj MakefileGenerator::foo() const
{ return QStringLiteral("yoyoyo"); }

int main()
{
  MakefileGenerator m;
  const Obj o = m.foo();
  return o.d[1];
}
