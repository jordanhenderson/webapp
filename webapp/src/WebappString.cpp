#include "Platform.h"
#include "WebappString.h"

webapp_str_t operator+(const webapp_str_t& w1, const webapp_str_t& w2)
{
	webapp_str_t n = w1;
	n += w2;
	return n;
}

webapp_str_t operator+(const char* lhs, const webapp_str_t& rhs)
{
	return webapp_str_t(lhs) + rhs;
}

webapp_str_t operator+(const webapp_str_t& lhs, const char* rhs)
{
	return lhs + webapp_str_t(rhs);
}
 
