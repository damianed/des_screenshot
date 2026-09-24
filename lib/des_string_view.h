#include <stdbool.h>

typedef struct {
    char         *data;
    unsigned int  size;
} StringView;

StringView strToStringView(char *s);

bool strEquals(char *s1, char *s2);
bool strViewEquals(StringView *s1, StringView *s2);
bool strViewStartsWith(StringView *s, StringView *subs);

void strViewTrimCharsLeft(StringView *s, int n);
void strViewTrimCharsRight(StringView *s, int n);

/*
 * Returns first section of resulting split and makes param "s" the second section
 * StringView sv = strToStringView("data=user_name");
 * eg. strViewSplit(&sv, '=');
 * returns "data" as a StringView struct and makes sv = user_name as a different StringView struct
 */
StringView strViewSplit(StringView *s, char separator);
