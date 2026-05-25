#pragma once
#include <CRT\crt.hpp>

template <typename T>
class SimpleVector {
private:
    T* data;
    size_t v_size;
    size_t v_capacity;

    T* reallocate(size_t new_capacity) {
        T *p = (T*)realloc(data, sizeof(T) * new_capacity);
        if (p) {
            data = p;
            v_capacity = new_capacity;
        }
        return p;
    }

public:
    SimpleVector() : data(nullptr), v_size(0), v_capacity(0) {}

    ~SimpleVector() { RemoveAll(); free(data); }

    T* Data() {return data;}

    void Add(const T& value) {
        T *p;
        if (v_size >= v_capacity)
            p = reallocate(v_capacity == 0 ? 1 : v_capacity * 2);
        else
            p = data;
        if (p)
            p[v_size++] = value;
    }

    void RemoveAll() {
        for (size_t i = 0; i < v_size; ++i)
            delete data[i];
        v_size = 0;
    }

    T& operator[](size_t index) { return data[index]; }
    size_t size() const { return v_size; }
};
