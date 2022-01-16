#include <ctype.h>

int isupper(int c)
{
  return c >= 'A' && c <= 'Z';
}

int islower(int c)
{
  return c >= 'a' && c <= 'z';
}

int isalpha(int c)
{
  return islower(c) || isupper(c);
}

int isdigit(int c)
{
  return ((unsigned)c - '0') <= 9;
}

int isalnum(int c)
{
  return isalpha(c) || isdigit(c);
}

int isascii(int c)
{
  return !(c & ~0x7f);
}

int isblank(int c)
{
  return (c == '\t') || (c == ' ');
}

int iscntrl(int c)
{
  return c < 0x20;
}

int isspace(int c)
{
  return c == ' ' || c == '\n' || c == '\t' || c == '\r';
}

int isxdigit(int c)
{
  return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int toupper(int c)
{
  return islower(c) ? (c & ~32) : c;
}

int tolower(int c)
{
  return isupper(c) ? (c | 32) : c;
}
