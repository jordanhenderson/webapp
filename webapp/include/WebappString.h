#ifndef WEBAPPSTRING_H
#define WEBAPPSTRING_H
#include <ctemplate/template.h>
#include <leveldb/db.h>

struct _webapp_str_t {
    char* data = NULL;
    uint32_t len = 0;
    int allocated = 0;
};

struct webapp_str_t {
    char* data = NULL;
    uint32_t len = 0;
    int allocated = 1;
    webapp_str_t() {
        allocated = 0;
    }
    webapp_str_t(const char* s, uint32_t _len) {
        len = _len;
        data = new char[_len];
        memcpy(data, s, _len);
    }
    webapp_str_t(uint32_t _len) {
        len = _len;
        data = new char[_len];
    }
    webapp_str_t(const char* s) {
        len = strlen(s);
        data = new char[len];
        memcpy(data, s, len);
    }
    webapp_str_t(webapp_str_t* other) {
        if(other == NULL) {
            data = new char[1];
            len = 0;
        } else {
            len = other->len;
            data = other->data;
            allocated = other->allocated;
        }
    }
    webapp_str_t(const webapp_str_t& other) {
        len = other.len;
        data = new char[len];
        memcpy(data, other.data, len);
    }
    ~webapp_str_t() {
        if(allocated) delete[] data;
    }
    operator std::string const () const {
        return std::string(data, len);
    }
    operator ctemplate::TemplateString const () const {
        return ctemplate::TemplateString(data, len);
    }
    operator leveldb::Slice const () const {
        return leveldb::Slice(data, len);
    }
    webapp_str_t& operator +=(const webapp_str_t& other) {
        uint32_t newlen = len + other.len;
        char* r = new char[newlen];
        memcpy(r, data, len);
        memcpy(r + len, other.data, other.len);
        if(allocated) delete[] data;
        len = newlen;
        data = r;
        return *this;
    }
    webapp_str_t& operator=(const webapp_str_t& other) {
        if(this != &other) {
            char* r = new char[other.len];
            memcpy(r, other.data, other.len);
            if(allocated) delete[] data;
            data = r;
            len = other.len;
        }
        return *this;
    }
    friend webapp_str_t operator+(const webapp_str_t& w1, const webapp_str_t& w2);
    friend webapp_str_t operator+(const char* lhs, const webapp_str_t& rhs);
    friend webapp_str_t operator+(const webapp_str_t& lhs, const char* rhs);
    void from_number(int num) {
        if(allocated) delete[] data;
        data = new char[21];
        len = snprintf(data, 21, "%d", num);
    }
};

template <class T>
struct webapp_data_t : webapp_str_t {
    webapp_data_t(T _data) : webapp_str_t(sizeof(T)) {
        *(T*)(data) = _data;
    }
};
#endif // WEBAPPSTRING_H
