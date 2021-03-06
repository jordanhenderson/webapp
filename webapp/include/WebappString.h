#ifndef WEBAPPSTRING_H
#define WEBAPPSTRING_H
#include <leveldb/db.h>

struct _webapp_str_t {
	char* data = NULL;
	int32_t len = 0;
	int allocated = 0;
};

struct webapp_str_t : _webapp_str_t {
	webapp_str_t() {}
	webapp_str_t(const char* s, int32_t _len)
	{
		if(s != NULL) {
			len = _len;
			data = new char[_len];
			allocated = 1;
			memcpy(data, s, _len);
		}
	}
	webapp_str_t(int32_t _len)
	{
		len = _len;
		allocated = 1;
		data = new char[_len];
	}
	webapp_str_t(const char* s)
	{
		if(s != NULL) {
			/* Assume null terminated string */
			len = strlen(s);
			allocated = 1;
			data = new char[len + 1];
			memcpy(data, s, len + 1);
		}
	}
	webapp_str_t(const std::string& other)
	{
		len = other.size();
		allocated = 1;
		data = new char[len];
		memcpy(data, other.c_str(), len);
	}
	webapp_str_t(webapp_str_t* other)
	{
		if(other == NULL) {
			len = 0;
			allocated = 0;
		} else {
			len = other->len;
			data = other->data;
			allocated = 0; //Don't deallocate unmanaged memory.
		}
	}
	webapp_str_t(const webapp_str_t& other)
	{
		len = other.len;
		data = new char[len];
		allocated = 1;
		memcpy(data, other.data, len);
	}
	webapp_str_t(const leveldb::Slice& other)
	{
		len = other.size();
		data = new char[len];
		allocated = 1;
		memcpy(data, other.data(), len);
	}
	~webapp_str_t()
	{
		if(allocated) delete[] data;
	}
	operator std::string const () const
	{
		return std::string(data, len);
	}
	operator leveldb::Slice const () const
	{
		return leveldb::Slice(data, len);
	}

	webapp_str_t& operator +=(const webapp_str_t& other)
	{
		if(other.len > 0) {
			int32_t newlen = len + other.len;
			char* r = new char[newlen];
			memcpy(r, data, len);
			memcpy(r + len, other.data, other.len);
			if(allocated) delete[] data;
			len = newlen;
			data = r;
			allocated = 1;
		}
		return *this;
	}
	webapp_str_t& operator=(const webapp_str_t& other)
	{
		if(this != &other) {
			char* r = new char[other.len];
			memcpy(r, other.data, other.len);
			if(allocated) delete[] data;
			data = r;
			len = other.len;
			allocated = 1;
		}
		return *this;
	}
	friend webapp_str_t operator+(const webapp_str_t& w1, const webapp_str_t& w2);
	friend webapp_str_t operator+(const char* lhs, const webapp_str_t& rhs);
	friend webapp_str_t operator+(const webapp_str_t& lhs, const char* rhs);
	friend webapp_str_t operator+(const webapp_str_t& lhs, char c);
	void from_number(int num)
	{
		if(allocated) delete[] data;
		data = new char[21];
		len = snprintf(data, 21, "%d", num);
		allocated = 1;
	}
	void to_lower()
	{
		for(int i = 0; i < len; i++) {
			data[i] = tolower(data[i]);
		}
	}
	int endsWith(const webapp_str_t& other) {
		if(other.len > len) return 0;
		return (0 == strncmp(data + len - other.len, other.data, other.len));
	}
};

template <class T>
struct webapp_data_t : webapp_str_t {
	webapp_data_t(T _data) : webapp_str_t(sizeof(T))
	{
		*(T*)(data) = _data;
	}
};

#endif // WEBAPPSTRING_H
