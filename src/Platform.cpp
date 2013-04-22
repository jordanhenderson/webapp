#include "Platform.h"
using namespace std;
bool endsWith(const string &a, const string &b) {
	if (a.length() >= b.length()) {
		return (a.compare (a.length() - b.length(), b.length(), b) == 0);
	} else {
		return false;
	}
}