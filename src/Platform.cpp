#include "Platform.h"
bool endsWith(const tstring &a, const tstring &b) {
	if (a.length() >= b.length()) {
		return (a.compare (a.length() - b.length(), b.length(), b) == 0);
	} else {
		return false;
	}
}