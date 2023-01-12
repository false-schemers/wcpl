#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#define REQUIRE(x) do { ++testno; if (!(x)) fail(testno, buffer); } while(0)


static int testno = 0;
static int failcnt = 0;
void fail(int tno, const char *buf)
{
  printf("  test %d failed: buffer is [%s]\n", testno, buf);
  ++failcnt;
}

void start(const char *test)
{
  printf("\n** test set %s **\n", test);
  fflush(stdout);
  testno = 0;
}


/* printf tests */

void test_snprintf (void) {
  start("snprintf");
  char buffer[100];

  snprintf(buffer, 100U, "%d", -1000);
  REQUIRE(!strcmp(buffer, "-1000"));

  snprintf(buffer, 3U, "%d", -1000);
  REQUIRE(!strcmp(buffer, "-1"));
}

/*
static void vprintf_builder_1(char* buffer, ...)
{
  va_list args;
  va_start(args, buffer);
  vprintf("%d", args);
  va_end(args);
}

static void vsnprintf_builder_1(char* buffer, ...)
{
  va_list args;
  va_start(args, buffer);
  vsnprintf(buffer, 100U, "%d", args);
  va_end(args);
}

static void vsnprintf_builder_3(char* buffer, ...)
{
  va_list args;
  va_start(args, buffer);
  vsnprintf(buffer, 100U, "%d %d %s", args);
  va_end(args);
}


void test_vprintf (void) {
  start("vprintf");
  char buffer[100];
  printf_idx = 0U;
  memset(printf_buffer, 0xCC, 100U);
  vprintf_builder_1(buffer, 2345);
  REQUIRE(printf_buffer[4] == (char)0xCC);
  printf_buffer[4] = 0;
  REQUIRE(!strcmp(printf_buffer, "2345"));
}


void test_vsnprintf (void) {
  start("vsnprintf");
  char buffer[100];

  vsnprintf_builder_1(buffer, -1);
  REQUIRE(!strcmp(buffer, "-1"));

  vsnprintf_builder_3(buffer, 3, -1000, "test");
  REQUIRE(!strcmp(buffer, "3 -1000 test"));
}
*/

void test_space_flag(void) {
  start("test_space_flag");
  char buffer[100];

  sprintf(buffer, "% d", 42);
  REQUIRE(!strcmp(buffer, " 42"));

  sprintf(buffer, "% d", -42);
  REQUIRE(!strcmp(buffer, "-42"));

  sprintf(buffer, "% 5d", 42);
  REQUIRE(!strcmp(buffer, "   42"));

  sprintf(buffer, "% 5d", -42);
  REQUIRE(!strcmp(buffer, "  -42"));

  sprintf(buffer, "% 15d", 42);
  REQUIRE(!strcmp(buffer, "             42"));

  sprintf(buffer, "% 15d", -42);
  REQUIRE(!strcmp(buffer, "            -42"));

  sprintf(buffer, "% 15d", -42);
  REQUIRE(!strcmp(buffer, "            -42"));

  sprintf(buffer, "% 15.3f", -42.987);
  REQUIRE(!strcmp(buffer, "        -42.987"));

  sprintf(buffer, "% 15.3f", 42.987);
  REQUIRE(!strcmp(buffer, "         42.987"));

  sprintf(buffer, "% s", "Hello testing");
  REQUIRE(!strcmp(buffer, "Hello testing"));

  sprintf(buffer, "% d", 1024);
  REQUIRE(!strcmp(buffer, " 1024"));

  sprintf(buffer, "% d", -1024);
  REQUIRE(!strcmp(buffer, "-1024"));

  sprintf(buffer, "% i", 1024);
  REQUIRE(!strcmp(buffer, " 1024"));

  sprintf(buffer, "% i", -1024);
  REQUIRE(!strcmp(buffer, "-1024"));

  sprintf(buffer, "% u", 1024);
  REQUIRE(!strcmp(buffer, "1024"));

  sprintf(buffer, "% u", 4294966272U);
  REQUIRE(!strcmp(buffer, "4294966272"));

  sprintf(buffer, "% o", 511);
  REQUIRE(!strcmp(buffer, "777"));

  sprintf(buffer, "% o", 4294966785U);
  REQUIRE(!strcmp(buffer, "37777777001"));

  sprintf(buffer, "% x", 305441741);
  REQUIRE(!strcmp(buffer, "1234abcd"));

  sprintf(buffer, "% x", 3989525555U);
  REQUIRE(!strcmp(buffer, "edcb5433"));

  sprintf(buffer, "% X", 305441741);
  REQUIRE(!strcmp(buffer, "1234ABCD"));

  sprintf(buffer, "% X", 3989525555U);
  REQUIRE(!strcmp(buffer, "EDCB5433"));

  sprintf(buffer, "% c", 'x');
  REQUIRE(!strcmp(buffer, "x"));
}


void test_plus_flag (void) {
  start("plus_flag");
  char buffer[100];

  sprintf(buffer, "%+d", 42);
  REQUIRE(!strcmp(buffer, "+42"));

  sprintf(buffer, "%+d", -42);
  REQUIRE(!strcmp(buffer, "-42"));

  sprintf(buffer, "%+5d", 42);
  REQUIRE(!strcmp(buffer, "  +42"));

  sprintf(buffer, "%+5d", -42);
  REQUIRE(!strcmp(buffer, "  -42"));

  sprintf(buffer, "%+15d", 42);
  REQUIRE(!strcmp(buffer, "            +42"));

  sprintf(buffer, "%+15d", -42);
  REQUIRE(!strcmp(buffer, "            -42"));

  sprintf(buffer, "%+s", "Hello testing");
  REQUIRE(!strcmp(buffer, "Hello testing"));

  sprintf(buffer, "%+d", 1024);
  REQUIRE(!strcmp(buffer, "+1024"));

  sprintf(buffer, "%+d", -1024);
  REQUIRE(!strcmp(buffer, "-1024"));

  sprintf(buffer, "%+i", 1024);
  REQUIRE(!strcmp(buffer, "+1024"));

  sprintf(buffer, "%+i", -1024);
  REQUIRE(!strcmp(buffer, "-1024"));

  sprintf(buffer, "%+u", 1024);
  REQUIRE(!strcmp(buffer, "1024"));

  sprintf(buffer, "%+u", 4294966272U);
  REQUIRE(!strcmp(buffer, "4294966272"));

  sprintf(buffer, "%+o", 511);
  REQUIRE(!strcmp(buffer, "777"));

  sprintf(buffer, "%+o", 4294966785U);
  REQUIRE(!strcmp(buffer, "37777777001"));

  sprintf(buffer, "%+x", 305441741);
  REQUIRE(!strcmp(buffer, "1234abcd"));

  sprintf(buffer, "%+x", 3989525555U);
  REQUIRE(!strcmp(buffer, "edcb5433"));

  sprintf(buffer, "%+X", 305441741);
  REQUIRE(!strcmp(buffer, "1234ABCD"));

  sprintf(buffer, "%+X", 3989525555U);
  REQUIRE(!strcmp(buffer, "EDCB5433"));

  sprintf(buffer, "%+c", 'x');
  REQUIRE(!strcmp(buffer, "x"));

  sprintf(buffer, "%+.0d", 0);
  REQUIRE(!strcmp(buffer, "+"));
}


void test_0_flag (void) {
  start("0_flag");
  char buffer[100];

  sprintf(buffer, "%0d", 42);
  REQUIRE(!strcmp(buffer, "42"));

  sprintf(buffer, "%0ld", 42L);
  REQUIRE(!strcmp(buffer, "42"));

  sprintf(buffer, "%0d", -42);
  REQUIRE(!strcmp(buffer, "-42"));

  sprintf(buffer, "%05d", 42);
  REQUIRE(!strcmp(buffer, "00042"));

  sprintf(buffer, "%05d", -42);
  REQUIRE(!strcmp(buffer, "-0042"));

  sprintf(buffer, "%015d", 42);
  REQUIRE(!strcmp(buffer, "000000000000042"));

  sprintf(buffer, "%015d", -42);
  REQUIRE(!strcmp(buffer, "-00000000000042"));

  sprintf(buffer, "%015.2f", 42.1234);
  REQUIRE(!strcmp(buffer, "000000000042.12"));

  sprintf(buffer, "%015.3f", 42.9876);
  REQUIRE(!strcmp(buffer, "00000000042.988"));

  sprintf(buffer, "%015.5f", -42.9876);
  REQUIRE(!strcmp(buffer, "-00000042.98760"));
}


void test_minus_flag (void) {
  start("minus_flag");
  char buffer[100];

  sprintf(buffer, "%-d", 42);
  REQUIRE(!strcmp(buffer, "42"));

  sprintf(buffer, "%-d", -42);
  REQUIRE(!strcmp(buffer, "-42"));

  sprintf(buffer, "%-5d", 42);
  REQUIRE(!strcmp(buffer, "42   "));

  sprintf(buffer, "%-5d", -42);
  REQUIRE(!strcmp(buffer, "-42  "));

  sprintf(buffer, "%-15d", 42);
  REQUIRE(!strcmp(buffer, "42             "));

  sprintf(buffer, "%-15d", -42);
  REQUIRE(!strcmp(buffer, "-42            "));

  sprintf(buffer, "%-0d", 42);
  REQUIRE(!strcmp(buffer, "42"));

  sprintf(buffer, "%-0d", -42);
  REQUIRE(!strcmp(buffer, "-42"));

  sprintf(buffer, "%-05d", 42);
  REQUIRE(!strcmp(buffer, "42   "));

  sprintf(buffer, "%-05d", -42);
  REQUIRE(!strcmp(buffer, "-42  "));

  sprintf(buffer, "%-015d", 42);
  REQUIRE(!strcmp(buffer, "42             "));

  sprintf(buffer, "%-015d", -42);
  REQUIRE(!strcmp(buffer, "-42            "));

  sprintf(buffer, "%0-d", 42);
  REQUIRE(!strcmp(buffer, "42"));

  sprintf(buffer, "%0-d", -42);
  REQUIRE(!strcmp(buffer, "-42"));

  sprintf(buffer, "%0-5d", 42);
  REQUIRE(!strcmp(buffer, "42   "));

  sprintf(buffer, "%0-5d", -42);
  REQUIRE(!strcmp(buffer, "-42  "));

  sprintf(buffer, "%0-15d", 42);
  REQUIRE(!strcmp(buffer, "42             "));

  sprintf(buffer, "%0-15d", -42);
  REQUIRE(!strcmp(buffer, "-42            "));

  sprintf(buffer, "%0-15.3e", -42.);
  REQUIRE(!strcmp(buffer, "-4.200e+01     "));

  sprintf(buffer, "%0-15.3g", -42.);
  REQUIRE(!strcmp(buffer, "-42.0          "));
}


void test_hash_flag (void) {
  start("hash_flag");
  char buffer[100];

  sprintf(buffer, "%#.0x", 0);
  REQUIRE(!strcmp(buffer, ""));
  sprintf(buffer, "%#.1x", 0);
  REQUIRE(!strcmp(buffer, "0"));
  sprintf(buffer, "%#.0llx", (long long)0);
  REQUIRE(!strcmp(buffer, ""));
  sprintf(buffer, "%#.8x", 0x614e);
  REQUIRE(!strcmp(buffer, "0x0000614e"));
  sprintf(buffer,"%#b", 6);
  REQUIRE(!strcmp(buffer, "0b110"));
}


void test_specifier (void) {
  start("specifier");
  char buffer[100];

  sprintf(buffer, "Hello testing");
  REQUIRE(!strcmp(buffer, "Hello testing"));

  sprintf(buffer, "%s", "Hello testing");
  REQUIRE(!strcmp(buffer, "Hello testing"));

  sprintf(buffer, "%d", 1024);
  REQUIRE(!strcmp(buffer, "1024"));

  sprintf(buffer, "%d", -1024);
  REQUIRE(!strcmp(buffer, "-1024"));

  sprintf(buffer, "%i", 1024);
  REQUIRE(!strcmp(buffer, "1024"));

  sprintf(buffer, "%i", -1024);
  REQUIRE(!strcmp(buffer, "-1024"));

  sprintf(buffer, "%u", 1024);
  REQUIRE(!strcmp(buffer, "1024"));

  sprintf(buffer, "%u", 4294966272U);
  REQUIRE(!strcmp(buffer, "4294966272"));

  sprintf(buffer, "%o", 511);
  REQUIRE(!strcmp(buffer, "777"));

  sprintf(buffer, "%o", 4294966785U);
  REQUIRE(!strcmp(buffer, "37777777001"));

  sprintf(buffer, "%x", 305441741);
  REQUIRE(!strcmp(buffer, "1234abcd"));

  sprintf(buffer, "%x", 3989525555U);
  REQUIRE(!strcmp(buffer, "edcb5433"));

  sprintf(buffer, "%X", 305441741);
  REQUIRE(!strcmp(buffer, "1234ABCD"));

  sprintf(buffer, "%X", 3989525555U);
  REQUIRE(!strcmp(buffer, "EDCB5433"));

  sprintf(buffer, "%%");
  REQUIRE(!strcmp(buffer, "%"));
}


void test_width (void) {
  start("width");
  char buffer[100];

  sprintf(buffer, "%1s", "Hello testing");
  REQUIRE(!strcmp(buffer, "Hello testing"));

  sprintf(buffer, "%1d", 1024);
  REQUIRE(!strcmp(buffer, "1024"));

  sprintf(buffer, "%1d", -1024);
  REQUIRE(!strcmp(buffer, "-1024"));

  sprintf(buffer, "%1i", 1024);
  REQUIRE(!strcmp(buffer, "1024"));

  sprintf(buffer, "%1i", -1024);
  REQUIRE(!strcmp(buffer, "-1024"));

  sprintf(buffer, "%1u", 1024);
  REQUIRE(!strcmp(buffer, "1024"));

  sprintf(buffer, "%1u", 4294966272U);
  REQUIRE(!strcmp(buffer, "4294966272"));

  sprintf(buffer, "%1o", 511);
  REQUIRE(!strcmp(buffer, "777"));

  sprintf(buffer, "%1o", 4294966785U);
  REQUIRE(!strcmp(buffer, "37777777001"));

  sprintf(buffer, "%1x", 305441741);
  REQUIRE(!strcmp(buffer, "1234abcd"));

  sprintf(buffer, "%1x", 3989525555U);
  REQUIRE(!strcmp(buffer, "edcb5433"));

  sprintf(buffer, "%1X", 305441741);
  REQUIRE(!strcmp(buffer, "1234ABCD"));

  sprintf(buffer, "%1X", 3989525555U);
  REQUIRE(!strcmp(buffer, "EDCB5433"));

  sprintf(buffer, "%1c", 'x');
  REQUIRE(!strcmp(buffer, "x"));
}


void test_width_20 (void) {
  start("width_20");
  char buffer[100];

  sprintf(buffer, "%20s", "Hello");
  REQUIRE(!strcmp(buffer, "               Hello"));

  sprintf(buffer, "%20d", 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%20d", -1024);
  REQUIRE(!strcmp(buffer, "               -1024"));

  sprintf(buffer, "%20i", 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%20i", -1024);
  REQUIRE(!strcmp(buffer, "               -1024"));

  sprintf(buffer, "%20u", 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%20u", 4294966272U);
  REQUIRE(!strcmp(buffer, "          4294966272"));

  sprintf(buffer, "%20o", 511);
  REQUIRE(!strcmp(buffer, "                 777"));

  sprintf(buffer, "%20o", 4294966785U);
  REQUIRE(!strcmp(buffer, "         37777777001"));

  sprintf(buffer, "%20x", 305441741);
  REQUIRE(!strcmp(buffer, "            1234abcd"));

  sprintf(buffer, "%20x", 3989525555U);
  REQUIRE(!strcmp(buffer, "            edcb5433"));

  sprintf(buffer, "%20X", 305441741);
  REQUIRE(!strcmp(buffer, "            1234ABCD"));

  sprintf(buffer, "%20X", 3989525555U);
  REQUIRE(!strcmp(buffer, "            EDCB5433"));

  sprintf(buffer, "%20c", 'x');
  REQUIRE(!strcmp(buffer, "                   x"));
}


void test_width_star_20 (void) {
  start("width_star_20");
  char buffer[100];

  sprintf(buffer, "%*s", 20, "Hello");
  REQUIRE(!strcmp(buffer, "               Hello"));

  sprintf(buffer, "%*d", 20, 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%*d", 20, -1024);
  REQUIRE(!strcmp(buffer, "               -1024"));

  sprintf(buffer, "%*i", 20, 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%*i", 20, -1024);
  REQUIRE(!strcmp(buffer, "               -1024"));

  sprintf(buffer, "%*u", 20, 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%*u", 20, 4294966272U);
  REQUIRE(!strcmp(buffer, "          4294966272"));

  sprintf(buffer, "%*o", 20, 511);
  REQUIRE(!strcmp(buffer, "                 777"));

  sprintf(buffer, "%*o", 20, 4294966785U);
  REQUIRE(!strcmp(buffer, "         37777777001"));

  sprintf(buffer, "%*x", 20, 305441741);
  REQUIRE(!strcmp(buffer, "            1234abcd"));

  sprintf(buffer, "%*x", 20, 3989525555U);
  REQUIRE(!strcmp(buffer, "            edcb5433"));

  sprintf(buffer, "%*X", 20, 305441741);
  REQUIRE(!strcmp(buffer, "            1234ABCD"));

  sprintf(buffer, "%*X", 20, 3989525555U);
  REQUIRE(!strcmp(buffer, "            EDCB5433"));

  sprintf(buffer, "%*c", 20,'x');
  REQUIRE(!strcmp(buffer, "                   x"));
}


void test_width_minus_20 (void) {
  start("width_minus_20");
  char buffer[100];

  sprintf(buffer, "%-20s", "Hello");
  REQUIRE(!strcmp(buffer, "Hello               "));

  sprintf(buffer, "%-20d", 1024);
  REQUIRE(!strcmp(buffer, "1024                "));

  sprintf(buffer, "%-20d", -1024);
  REQUIRE(!strcmp(buffer, "-1024               "));

  sprintf(buffer, "%-20i", 1024);
  REQUIRE(!strcmp(buffer, "1024                "));

  sprintf(buffer, "%-20i", -1024);
  REQUIRE(!strcmp(buffer, "-1024               "));

  sprintf(buffer, "%-20u", 1024);
  REQUIRE(!strcmp(buffer, "1024                "));

  sprintf(buffer, "%-20.4f", 1024.1234);
  REQUIRE(!strcmp(buffer, "1024.1234           "));

  sprintf(buffer, "%-20u", 4294966272U);
  REQUIRE(!strcmp(buffer, "4294966272          "));

  sprintf(buffer, "%-20o", 511);
  REQUIRE(!strcmp(buffer, "777                 "));

  sprintf(buffer, "%-20o", 4294966785U);
  REQUIRE(!strcmp(buffer, "37777777001         "));

  sprintf(buffer, "%-20x", 305441741);
  REQUIRE(!strcmp(buffer, "1234abcd            "));

  sprintf(buffer, "%-20x", 3989525555U);
  REQUIRE(!strcmp(buffer, "edcb5433            "));

  sprintf(buffer, "%-20X", 305441741);
  REQUIRE(!strcmp(buffer, "1234ABCD            "));

  sprintf(buffer, "%-20X", 3989525555U);
  REQUIRE(!strcmp(buffer, "EDCB5433            "));

  sprintf(buffer, "%-20c", 'x');
  REQUIRE(!strcmp(buffer, "x                   "));

  sprintf(buffer, "|%5d| |%-2d| |%5d|", 9, 9, 9);
  REQUIRE(!strcmp(buffer, "|    9| |9 | |    9|"));

  sprintf(buffer, "|%5d| |%-2d| |%5d|", 10, 10, 10);
  REQUIRE(!strcmp(buffer, "|   10| |10| |   10|"));

  sprintf(buffer, "|%5d| |%-12d| |%5d|", 9, 9, 9);
  REQUIRE(!strcmp(buffer, "|    9| |9           | |    9|"));

  sprintf(buffer, "|%5d| |%-12d| |%5d|", 10, 10, 10);
  REQUIRE(!strcmp(buffer, "|   10| |10          | |   10|"));
}


void test_width_0_20 (void) {
  start("width_0_20");
  char buffer[100];

  sprintf(buffer, "%0-20s", "Hello");
  REQUIRE(!strcmp(buffer, "Hello               "));

  sprintf(buffer, "%0-20d", 1024);
  REQUIRE(!strcmp(buffer, "1024                "));

  sprintf(buffer, "%0-20d", -1024);
  REQUIRE(!strcmp(buffer, "-1024               "));

  sprintf(buffer, "%0-20i", 1024);
  REQUIRE(!strcmp(buffer, "1024                "));

  sprintf(buffer, "%0-20i", -1024);
  REQUIRE(!strcmp(buffer, "-1024               "));

  sprintf(buffer, "%0-20u", 1024);
  REQUIRE(!strcmp(buffer, "1024                "));

  sprintf(buffer, "%0-20u", 4294966272U);
  REQUIRE(!strcmp(buffer, "4294966272          "));

  sprintf(buffer, "%0-20o", 511);
  REQUIRE(!strcmp(buffer, "777                 "));

  sprintf(buffer, "%0-20o", 4294966785U);
  REQUIRE(!strcmp(buffer, "37777777001         "));

  sprintf(buffer, "%0-20x", 305441741);
  REQUIRE(!strcmp(buffer, "1234abcd            "));

  sprintf(buffer, "%0-20x", 3989525555U);
  REQUIRE(!strcmp(buffer, "edcb5433            "));

  sprintf(buffer, "%0-20X", 305441741);
  REQUIRE(!strcmp(buffer, "1234ABCD            "));

  sprintf(buffer, "%0-20X", 3989525555U);
  REQUIRE(!strcmp(buffer, "EDCB5433            "));

  sprintf(buffer, "%0-20c", 'x');
  REQUIRE(!strcmp(buffer, "x                   "));
}


void test_padding_20 (void) {
  start("padding_20");
  char buffer[100];

  sprintf(buffer, "%020d", 1024);
  REQUIRE(!strcmp(buffer, "00000000000000001024"));

  sprintf(buffer, "%020d", -1024);
  REQUIRE(!strcmp(buffer, "-0000000000000001024"));

  sprintf(buffer, "%020i", 1024);
  REQUIRE(!strcmp(buffer, "00000000000000001024"));

  sprintf(buffer, "%020i", -1024);
  REQUIRE(!strcmp(buffer, "-0000000000000001024"));

  sprintf(buffer, "%020u", 1024);
  REQUIRE(!strcmp(buffer, "00000000000000001024"));

  sprintf(buffer, "%020u", 4294966272U);
  REQUIRE(!strcmp(buffer, "00000000004294966272"));

  sprintf(buffer, "%020o", 511);
  REQUIRE(!strcmp(buffer, "00000000000000000777"));

  sprintf(buffer, "%020o", 4294966785U);
  REQUIRE(!strcmp(buffer, "00000000037777777001"));

  sprintf(buffer, "%020x", 305441741);
  REQUIRE(!strcmp(buffer, "0000000000001234abcd"));

  sprintf(buffer, "%020x", 3989525555U);
  REQUIRE(!strcmp(buffer, "000000000000edcb5433"));

  sprintf(buffer, "%020X", 305441741);
  REQUIRE(!strcmp(buffer, "0000000000001234ABCD"));

  sprintf(buffer, "%020X", 3989525555U);
  REQUIRE(!strcmp(buffer, "000000000000EDCB5433"));
}


void test_padding_dot_20 (void) {
  start("padding_dot_20");
  char buffer[100];

  sprintf(buffer, "%.20d", 1024);
  REQUIRE(!strcmp(buffer, "00000000000000001024"));

  sprintf(buffer, "%.20d", -1024);
  REQUIRE(!strcmp(buffer, "-00000000000000001024"));

  sprintf(buffer, "%.20i", 1024);
  REQUIRE(!strcmp(buffer, "00000000000000001024"));

  sprintf(buffer, "%.20i", -1024);
  REQUIRE(!strcmp(buffer, "-00000000000000001024"));

  sprintf(buffer, "%.20u", 1024);
  REQUIRE(!strcmp(buffer, "00000000000000001024"));

  sprintf(buffer, "%.20u", 4294966272U);
  REQUIRE(!strcmp(buffer, "00000000004294966272"));

  sprintf(buffer, "%.20o", 511);
  REQUIRE(!strcmp(buffer, "00000000000000000777"));

  sprintf(buffer, "%.20o", 4294966785U);
  REQUIRE(!strcmp(buffer, "00000000037777777001"));

  sprintf(buffer, "%.20x", 305441741);
  REQUIRE(!strcmp(buffer, "0000000000001234abcd"));

  sprintf(buffer, "%.20x", 3989525555U);
  REQUIRE(!strcmp(buffer, "000000000000edcb5433"));

  sprintf(buffer, "%.20X", 305441741);
  REQUIRE(!strcmp(buffer, "0000000000001234ABCD"));

  sprintf(buffer, "%.20X", 3989525555U);
  REQUIRE(!strcmp(buffer, "000000000000EDCB5433"));
}


void test_padding_hash_020 (void) {
  start("padding_hash_020");
  char buffer[100];

  sprintf(buffer, "%#020d", 1024);
  REQUIRE(!strcmp(buffer, "00000000000000001024"));

  sprintf(buffer, "%#020d", -1024);
  REQUIRE(!strcmp(buffer, "-0000000000000001024"));

  sprintf(buffer, "%#020i", 1024);
  REQUIRE(!strcmp(buffer, "00000000000000001024"));

  sprintf(buffer, "%#020i", -1024);
  REQUIRE(!strcmp(buffer, "-0000000000000001024"));

  sprintf(buffer, "%#020u", 1024);
  REQUIRE(!strcmp(buffer, "00000000000000001024"));

  sprintf(buffer, "%#020u", 4294966272U);
  REQUIRE(!strcmp(buffer, "00000000004294966272"));

  sprintf(buffer, "%#020o", 511);
  REQUIRE(!strcmp(buffer, "00000000000000000777"));

  sprintf(buffer, "%#020o", 4294966785U);
  REQUIRE(!strcmp(buffer, "00000000037777777001"));

  sprintf(buffer, "%#020x", 305441741);
  REQUIRE(!strcmp(buffer, "0x00000000001234abcd"));

  sprintf(buffer, "%#020x", 3989525555U);
  REQUIRE(!strcmp(buffer, "0x0000000000edcb5433"));

  sprintf(buffer, "%#020X", 305441741);
  REQUIRE(!strcmp(buffer, "0X00000000001234ABCD"));

  sprintf(buffer, "%#020X", 3989525555U);
  REQUIRE(!strcmp(buffer, "0X0000000000EDCB5433"));
}


void test_padding_hash_20 (void) {
  start("padding_hash_20");
  char buffer[100];

  sprintf(buffer, "%#20d", 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%#20d", -1024);
  REQUIRE(!strcmp(buffer, "               -1024"));

  sprintf(buffer, "%#20i", 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%#20i", -1024);
  REQUIRE(!strcmp(buffer, "               -1024"));

  sprintf(buffer, "%#20u", 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%#20u", 4294966272U);
  REQUIRE(!strcmp(buffer, "          4294966272"));

  sprintf(buffer, "%#20o", 511);
  REQUIRE(!strcmp(buffer, "                0777"));

  sprintf(buffer, "%#20o", 4294966785U);
  REQUIRE(!strcmp(buffer, "        037777777001"));

  sprintf(buffer, "%#20x", 305441741);
  REQUIRE(!strcmp(buffer, "          0x1234abcd"));

  sprintf(buffer, "%#20x", 3989525555U);
  REQUIRE(!strcmp(buffer, "          0xedcb5433"));

  sprintf(buffer, "%#20X", 305441741);
  REQUIRE(!strcmp(buffer, "          0X1234ABCD"));

  sprintf(buffer, "%#20X", 3989525555U);
  REQUIRE(!strcmp(buffer, "          0XEDCB5433"));
}


void test_padding_20_dot_5 (void) {
  start("padding_20_dot_5");
  char buffer[100];

  sprintf(buffer, "%20.5d", 1024);
  REQUIRE(!strcmp(buffer, "               01024"));

  sprintf(buffer, "%20.5d", -1024);
  REQUIRE(!strcmp(buffer, "              -01024"));

  sprintf(buffer, "%20.5i", 1024);
  REQUIRE(!strcmp(buffer, "               01024"));

  sprintf(buffer, "%20.5i", -1024);
  REQUIRE(!strcmp(buffer, "              -01024"));

  sprintf(buffer, "%20.5u", 1024);
  REQUIRE(!strcmp(buffer, "               01024"));

  sprintf(buffer, "%20.5u", 4294966272U);
  REQUIRE(!strcmp(buffer, "          4294966272"));

  sprintf(buffer, "%20.5o", 511);
  REQUIRE(!strcmp(buffer, "               00777"));

  sprintf(buffer, "%20.5o", 4294966785U);
  REQUIRE(!strcmp(buffer, "         37777777001"));

  sprintf(buffer, "%20.5x", 305441741);
  REQUIRE(!strcmp(buffer, "            1234abcd"));

  sprintf(buffer, "%20.10x", 3989525555U);
  REQUIRE(!strcmp(buffer, "          00edcb5433"));

  sprintf(buffer, "%20.5X", 305441741);
  REQUIRE(!strcmp(buffer, "            1234ABCD"));

  sprintf(buffer, "%20.10X", 3989525555U);
  REQUIRE(!strcmp(buffer, "          00EDCB5433"));
}


void test_padding_neg_numbers (void) {
  start("padding_neg_numbers");
  char buffer[100];

  // space padding
  sprintf(buffer, "% 1d", -5);
  REQUIRE(!strcmp(buffer, "-5"));

  sprintf(buffer, "% 2d", -5);
  REQUIRE(!strcmp(buffer, "-5"));

  sprintf(buffer, "% 3d", -5);
  REQUIRE(!strcmp(buffer, " -5"));

  sprintf(buffer, "% 4d", -5);
  REQUIRE(!strcmp(buffer, "  -5"));

  // zero padding
  sprintf(buffer, "%01d", -5);
  REQUIRE(!strcmp(buffer, "-5"));

  sprintf(buffer, "%02d", -5);
  REQUIRE(!strcmp(buffer, "-5"));

  sprintf(buffer, "%03d", -5);
  REQUIRE(!strcmp(buffer, "-05"));

  sprintf(buffer, "%04d", -5);
  REQUIRE(!strcmp(buffer, "-005"));
}


void test_float_padding_neg_numbers (void) {
  start("float_padding_neg_numbers");
  char buffer[100];

  // space padding
  sprintf(buffer, "% 3.1f", -5.);
  REQUIRE(!strcmp(buffer, "-5.0"));

  sprintf(buffer, "% 4.1f", -5.);
  REQUIRE(!strcmp(buffer, "-5.0"));

  sprintf(buffer, "% 5.1f", -5.);
  REQUIRE(!strcmp(buffer, " -5.0"));

  sprintf(buffer, "% 6.1g", -5.);
  REQUIRE(!strcmp(buffer, "    -5"));

  sprintf(buffer, "% 6.1e", -5.);
  REQUIRE(!strcmp(buffer, "-5.0e+00"));

  sprintf(buffer, "% 10.1e", -5.);
  REQUIRE(!strcmp(buffer, "  -5.0e+00"));

  // zero padding
  sprintf(buffer, "%03.1f", -5.);
  REQUIRE(!strcmp(buffer, "-5.0"));

  sprintf(buffer, "%04.1f", -5.);
  REQUIRE(!strcmp(buffer, "-5.0"));

  sprintf(buffer, "%05.1f", -5.);
  REQUIRE(!strcmp(buffer, "-05.0"));

  // zero padding no decimal point
  sprintf(buffer, "%01.0f", -5.);
  REQUIRE(!strcmp(buffer, "-5"));

  sprintf(buffer, "%02.0f", -5.);
  REQUIRE(!strcmp(buffer, "-5"));

  sprintf(buffer, "%03.0f", -5.);
  REQUIRE(!strcmp(buffer, "-05"));

  sprintf(buffer, "%010.1e", -5.);
  REQUIRE(!strcmp(buffer, "-005.0e+00"));

  sprintf(buffer, "%07.0E", -5.);
  REQUIRE(!strcmp(buffer, "-05E+00"));

  sprintf(buffer, "%03.0g", -5.);
  REQUIRE(!strcmp(buffer, "-05"));
}

void test_length (void) {
  start("length");
  char buffer[100];

  sprintf(buffer, "%.0s", "Hello testing");
  REQUIRE(!strcmp(buffer, ""));

  sprintf(buffer, "%20.0s", "Hello testing");
  REQUIRE(!strcmp(buffer, "                    "));

  sprintf(buffer, "%.s", "Hello testing");
  REQUIRE(!strcmp(buffer, ""));

  sprintf(buffer, "%20.s", "Hello testing");
  REQUIRE(!strcmp(buffer, "                    "));

  sprintf(buffer, "%20.0d", 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%20.0d", -1024);
  REQUIRE(!strcmp(buffer, "               -1024"));

  sprintf(buffer, "%20.d", 0);
  REQUIRE(!strcmp(buffer, "                    "));

  sprintf(buffer, "%20.0i", 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%20.i", -1024);
  REQUIRE(!strcmp(buffer, "               -1024"));

  sprintf(buffer, "%20.i", 0);
  REQUIRE(!strcmp(buffer, "                    "));

  sprintf(buffer, "%20.u", 1024);
  REQUIRE(!strcmp(buffer, "                1024"));

  sprintf(buffer, "%20.0u", 4294966272U);
  REQUIRE(!strcmp(buffer, "          4294966272"));

  sprintf(buffer, "%20.u", 0U);
  REQUIRE(!strcmp(buffer, "                    "));

  sprintf(buffer, "%20.o", 511);
  REQUIRE(!strcmp(buffer, "                 777"));

  sprintf(buffer, "%20.0o", 4294966785U);
  REQUIRE(!strcmp(buffer, "         37777777001"));

  sprintf(buffer, "%20.o", 0U);
  REQUIRE(!strcmp(buffer, "                    "));

  sprintf(buffer, "%20.x", 305441741);
  REQUIRE(!strcmp(buffer, "            1234abcd"));

  sprintf(buffer, "%50.x", 305441741);
  REQUIRE(!strcmp(buffer, "                                          1234abcd"));

  sprintf(buffer, "%50.x%10.u", 305441741, 12345);
  REQUIRE(!strcmp(buffer, "                                          1234abcd     12345"));

  sprintf(buffer, "%20.0x", 3989525555U);
  REQUIRE(!strcmp(buffer, "            edcb5433"));

  sprintf(buffer, "%20.x", 0U);
  REQUIRE(!strcmp(buffer, "                    "));

  sprintf(buffer, "%20.X", 305441741);
  REQUIRE(!strcmp(buffer, "            1234ABCD"));

  sprintf(buffer, "%20.0X", 3989525555U);
  REQUIRE(!strcmp(buffer, "            EDCB5433"));

  sprintf(buffer, "%20.X", 0U);
  REQUIRE(!strcmp(buffer, "                    "));

  sprintf(buffer, "%02.0u", 0U);
  REQUIRE(!strcmp(buffer, "  "));

  sprintf(buffer, "%02.0d", 0);
  REQUIRE(!strcmp(buffer, "  "));
}


void test_float (void) {
  start("float");
  char buffer[100];

  // test special-case floats using math.h macros
  sprintf(buffer, "%8f", NAN);
  REQUIRE(!strcmp(buffer, "     nan"));

  sprintf(buffer, "%8f", INFINITY);
  REQUIRE(!strcmp(buffer, "     inf"));

  sprintf(buffer, "%-8f", -INFINITY);
  REQUIRE(!strcmp(buffer, "-inf    "));

  sprintf(buffer, "%+8e", INFINITY);
  REQUIRE(!strcmp(buffer, "    +inf"));

  sprintf(buffer, "%.4f", 3.1415354);
  REQUIRE(!strcmp(buffer, "3.1415"));

  sprintf(buffer, "%.3f", 30343.1415354);
  REQUIRE(!strcmp(buffer, "30343.142"));

  sprintf(buffer, "%.0f", 34.1415354);
  REQUIRE(!strcmp(buffer, "34"));

  sprintf(buffer, "%.0f", 1.3);
  REQUIRE(!strcmp(buffer, "1"));

  sprintf(buffer, "%.0f", 1.55);
  REQUIRE(!strcmp(buffer, "2"));

  sprintf(buffer, "%.1f", 1.64);
  REQUIRE(!strcmp(buffer, "1.6"));

  sprintf(buffer, "%.2f", 42.8952);
  REQUIRE(!strcmp(buffer, "42.90"));

  sprintf(buffer, "%.9f", 42.8952);
  REQUIRE(!strcmp(buffer, "42.895200000"));

  sprintf(buffer, "%.10f", 42.895223);
  REQUIRE(!strcmp(buffer, "42.8952230000"));

  // this testcase checks, that the precision is truncated to 9 digits.
  // a perfect working float should return the whole number
  sprintf(buffer, "%.12f", 42.89522312345678);
  REQUIRE(!strcmp(buffer, "42.895223123000"));

  // this testcase checks, that the precision is truncated AND rounded to 9 digits.
  // a perfect working float should return the whole number
  sprintf(buffer, "%.12f", 42.89522387654321);
  REQUIRE(!strcmp(buffer, "42.895223877000"));

  sprintf(buffer, "%6.2f", 42.8952);
  REQUIRE(!strcmp(buffer, " 42.90"));

  sprintf(buffer, "%+6.2f", 42.8952);
  REQUIRE(!strcmp(buffer, "+42.90"));

  sprintf(buffer, "%+5.1f", 42.9252);
  REQUIRE(!strcmp(buffer, "+42.9"));

  sprintf(buffer, "%f", 42.5);
  REQUIRE(!strcmp(buffer, "42.500000"));

  sprintf(buffer, "%.1f", 42.5);
  REQUIRE(!strcmp(buffer, "42.5"));

  sprintf(buffer, "%f", 42167.0);
  REQUIRE(!strcmp(buffer, "42167.000000"));

  sprintf(buffer, "%.9f", -12345.987654321);
  REQUIRE(!strcmp(buffer, "-12345.987654321"));

  sprintf(buffer, "%.1f", 3.999);
  REQUIRE(!strcmp(buffer, "4.0"));

  sprintf(buffer, "%.0f", 3.5);
  REQUIRE(!strcmp(buffer, "4"));

  sprintf(buffer, "%.0f", 4.5);
  REQUIRE(!strcmp(buffer, "4"));

  sprintf(buffer, "%.0f", 3.49);
  REQUIRE(!strcmp(buffer, "3"));

  sprintf(buffer, "%.1f", 3.49);
  REQUIRE(!strcmp(buffer, "3.5"));

  sprintf(buffer, "a%-5.1f", 0.5);
  REQUIRE(!strcmp(buffer, "a0.5  "));

  sprintf(buffer, "a%-5.1fend", 0.5);
  REQUIRE(!strcmp(buffer, "a0.5  end"));

  sprintf(buffer, "%G", 12345.678);
  REQUIRE(!strcmp(buffer, "12345.7"));

  sprintf(buffer, "%.7G", 12345.678);
  REQUIRE(!strcmp(buffer, "12345.68"));

  sprintf(buffer, "%.5G", 123456789.);
  REQUIRE(!strcmp(buffer, "1.2346E+08"));

  sprintf(buffer, "%.6G", 12345.);
  REQUIRE(!strcmp(buffer, "12345.0"));

  sprintf(buffer, "%+12.4g", 123456789.);
  REQUIRE(!strcmp(buffer, "  +1.235e+08"));

  sprintf(buffer, "%.2G", 0.001234);
  REQUIRE(!strcmp(buffer, "0.0012"));

  sprintf(buffer, "%+10.4G", 0.001234);
  REQUIRE(!strcmp(buffer, " +0.001234"));

  sprintf(buffer, "%+012.4g", 0.00001234);
  REQUIRE(!strcmp(buffer, "+001.234e-05"));

  //sprintf(buffer, "%.3g", -1.2345e-308); // strtod fails on older systems
  //REQUIRE(!strcmp(buffer, "-1.23e-308"));

  //sprintf(buffer, "%+.3E", 1.23e+308); // strtod fails on older systems
  //REQUIRE(!strcmp(buffer, "+1.230E+308"));

  // out of range for float: should switch to exp notation if supported, else empty
  sprintf(buffer, "%.1f", 1E20);
  REQUIRE(!strcmp(buffer, "1.0e+20"));

  /* brute force float
  bool fail = false;
  std::stringstream str;
  str.precision(5);
  for (float i = -100000; i < 100000; i += 1) {
    sprintf(buffer, "%.5f", i / 10000);
    str.str("");
    str << std::fixed << i / 10000;
    fail = fail || !!strcmp(buffer, str.str().c_str());
  }
  REQUIRE(!fail); */

  /* brute force exp
  str.setf(std::ios::scientific, std::ios::floatfield);
  for (float i = -1e20; i < 1e20; i += 1e15) {
    sprintf(buffer, "%.5f", i);
    str.str("");
    str << i;
    fail = fail || !!strcmp(buffer, str.str().c_str());
  }
  REQUIRE(!fail); */
}

void test_float_hex(void) {
  start("float hex");
  char buffer[100];

  // test special-case floats using math.h macros
  sprintf(buffer, "%8a", NAN);
  REQUIRE(!strcmp(buffer, "     nan"));

  sprintf(buffer, "%-8a", -INFINITY);
  REQUIRE(!strcmp(buffer, "-inf    "));

  sprintf(buffer, "%+8a", INFINITY);
  REQUIRE(!strcmp(buffer, "    +inf"));

  sprintf(buffer, "%.4a", 3.1415354);
  REQUIRE(!strcmp(buffer, "0x1.921ep+1"));

  sprintf(buffer, "%.3a", 30343.1415354);
  REQUIRE(!strcmp(buffer, "0x1.da2p+14"));

  sprintf(buffer, "%.0a", 34.1415354);
  REQUIRE(!strcmp(buffer, "0x1p+5"));

  sprintf(buffer, "%.0a", 1.3);
  REQUIRE(!strcmp(buffer, "0x1p+0"));

  sprintf(buffer, "%a", 1.55); /* 0x3FF8CCCCCCCCCCCD */
  REQUIRE(!strcmp(buffer, "0x1.8cccccccccccdp+0"));

  sprintf(buffer, "%.0a", 1.55);
  REQUIRE(!strcmp(buffer, "0x2p+0"));  // ms prints 0x1p+0, we print it as 0x2p+0, so does clang/wasm

  sprintf(buffer, "%.1a", 1.64);
  REQUIRE(!strcmp(buffer, "0x1.ap+0"));

  sprintf(buffer, "%.2a", 42.8952);
  REQUIRE(!strcmp(buffer, "0x1.57p+5"));

  sprintf(buffer, "%.9a", 42.8952);
  REQUIRE(!strcmp(buffer, "0x1.57295e9e2p+5"));

  sprintf(buffer, "%.10a", 42.895223);
  REQUIRE(!strcmp(buffer, "0x1.57296aad1dp+5"));

  sprintf(buffer, "%a", 42.89522312345678);
  REQUIRE(!strcmp(buffer, "0x1.57296abdaef57p+5"));

  sprintf(buffer, "%.12a", 42.89522387654321);
  REQUIRE(!strcmp(buffer, "0x1.57296b22c2d0p+5")); // clang/wasm prints 0x1.57296b22c2cfcp+5

  sprintf(buffer, "%6.2a", 42.8952);
  REQUIRE(!strcmp(buffer, "0x1.57p+5"));

  sprintf(buffer, "%+6.2a", 42.8952);
  REQUIRE(!strcmp(buffer, "+0x1.57p+5"));

  sprintf(buffer, "%+5.1a", 42.9252);
  REQUIRE(!strcmp(buffer, "+0x1.5p+5"));

  sprintf(buffer, "%a", 42.5);
  REQUIRE(!strcmp(buffer, "0x1.5400000000000p+5")); // clang/wasm prints "0x1.54p+5"

  sprintf(buffer, "%.1a", 42.5);
  REQUIRE(!strcmp(buffer, "0x1.5p+5"));

  sprintf(buffer, "%a", 42167.0);
  REQUIRE(!strcmp(buffer, "0x1.496e000000000p+15")); // clang/wasm prints "0x1.496ep+15"

  sprintf(buffer, "%.9a", -12345.987654321);
  REQUIRE(!strcmp(buffer, "-0x1.81cfe6b75p+13"));

  sprintf(buffer, "%.1a", 3.999);
  REQUIRE(!strcmp(buffer, "0x2.0p+1"));

  sprintf(buffer, "%.0a", 3.5);
  REQUIRE(!strcmp(buffer, "0x2p+1"));

  sprintf(buffer, "%.0a", 4.5);
  REQUIRE(!strcmp(buffer, "0x1p+2"));

  sprintf(buffer, "%.0a", 3.49);
  REQUIRE(!strcmp(buffer, "0x2p+1"));

  sprintf(buffer, "%.1a", 3.49);
  REQUIRE(!strcmp(buffer, "0x1.cp+1"));

  sprintf(buffer, "a%-5.1a", 0.5);
  REQUIRE(!strcmp(buffer, "a0x1.0p-1"));

  sprintf(buffer, "a%-5.1aend", 0.5);
  REQUIRE(!strcmp(buffer, "a0x1.0p-1end"));

  sprintf(buffer, "%A", 12345.678);
  REQUIRE(!strcmp(buffer, "0X1.81CD6C8B43958P+13"));

  sprintf(buffer, "%.7A", 12345.678);
  REQUIRE(!strcmp(buffer, "0X1.81CD6C9P+13"));

  sprintf(buffer, "%.5A", 123456789.);
  REQUIRE(!strcmp(buffer, "0X1.D6F34P+26"));

  sprintf(buffer, "%.6A", 12345.);
  REQUIRE(!strcmp(buffer, "0X1.81C800P+13"));

  sprintf(buffer, "%+12.4A", 123456789.);
  REQUIRE(!strcmp(buffer, "+0X1.D6F3P+26"));

  sprintf(buffer, "%A", 0.001234);
  REQUIRE(!strcmp(buffer, "0X1.437C5692B3CC5P-10"));

  sprintf(buffer, "%.2A", 0.001234);  /* 0x1.437c5692b3cc5p-10 = 0x3F5437C5692B3CC5 */
  REQUIRE(!strcmp(buffer, "0X1.43P-10"));

  sprintf(buffer, "%+10.4A", 0.001234);
  REQUIRE(!strcmp(buffer, "+0X1.437CP-10"));

  sprintf(buffer, "%+012.4A", 0.00001234);
  REQUIRE(!strcmp(buffer, "+0X1.9E10P-17"));

  //sprintf(buffer, "%A", -1.2345e-308); // could be read as -0X0.8E083A46497DDP-1022 or -0X0.8E083A46497DEP-1022
  //REQUIRE(!strcmp(buffer, "-0X0.8E083A46497DDP-1022")); // ms prints -0X0.8E083A46497DEP-1022, clang/wasm prints "-0X1.1C10748C92FBCP-1023"

  //sprintf(buffer, "%.3A", -1.2345e-308); /* -0x0.8e083a46497dep-1022 = 0x8008E083A46497DE */
  //REQUIRE(!strcmp(buffer, "-0X0.8E1P-1022")); // ms prints -0X0.8E0P-1022, we print -0X0.8E1P-1022,  clang/wasm prints -0X1.1C1P-1023

  //sprintf(buffer, "%A", 0x1.fffffffffffffp+0);
  //REQUIRE(!strcmp(buffer, "0X1.FFFFFFFFFFFFFP+0"));

  //sprintf(buffer, "%.3A", 0x1.fffffffffffffp+0);
  //REQUIRE(!strcmp(buffer, "0X2.000P+0"));

  sprintf(buffer, "%+.3A", 1.23e+308);
  REQUIRE(!strcmp(buffer, "+0X1.5E5P+1023"));
}


void test_types (void) {
  start("types");
  char buffer[100];

  sprintf(buffer, "%i", 0);
  REQUIRE(!strcmp(buffer, "0"));

  sprintf(buffer, "%i", 1234);
  REQUIRE(!strcmp(buffer, "1234"));

  sprintf(buffer, "%i", 32767);
  REQUIRE(!strcmp(buffer, "32767"));

  sprintf(buffer, "%i", -32767);
  REQUIRE(!strcmp(buffer, "-32767"));

  sprintf(buffer, "%li", 30L);
  REQUIRE(!strcmp(buffer, "30"));

  sprintf(buffer, "%li", -2147483647L);
  REQUIRE(!strcmp(buffer, "-2147483647"));

  sprintf(buffer, "%li", 2147483647L);
  REQUIRE(!strcmp(buffer, "2147483647"));

  sprintf(buffer, "%lli", 30LL);
  REQUIRE(!strcmp(buffer, "30"));

  sprintf(buffer, "%lli", -9223372036854775807LL);
  REQUIRE(!strcmp(buffer, "-9223372036854775807"));

  sprintf(buffer, "%lli", 9223372036854775807LL);
  REQUIRE(!strcmp(buffer, "9223372036854775807"));

  sprintf(buffer, "%lu", 100000L);
  REQUIRE(!strcmp(buffer, "100000"));

  sprintf(buffer, "%lu", 0xFFFFFFFFLU);
  REQUIRE(!strcmp(buffer, "4294967295"));

  sprintf(buffer, "%llu", 281474976710656LLU);
  REQUIRE(!strcmp(buffer, "281474976710656"));

  sprintf(buffer, "%llu", 18446744073709551615LLU);
  REQUIRE(!strcmp(buffer, "18446744073709551615"));

  sprintf(buffer, "%zu", 2147483647UL);
  REQUIRE(!strcmp(buffer, "2147483647"));

  sprintf(buffer, "%zd", 2147483647UL);
  REQUIRE(!strcmp(buffer, "2147483647"));

  if (sizeof(size_t) == sizeof(long)) {
    sprintf(buffer, "%zi", -2147483647L);
    REQUIRE(!strcmp(buffer, "-2147483647"));
  } else {
    sprintf(buffer, "%zi", -2147483647LL);
    REQUIRE(!strcmp(buffer, "-2147483647"));
  }

  sprintf(buffer, "%b", 60000);
  REQUIRE(!strcmp(buffer, "1110101001100000"));

  sprintf(buffer, "%lb", 12345678L);
  REQUIRE(!strcmp(buffer, "101111000110000101001110"));

  sprintf(buffer, "%o", 60000);
  REQUIRE(!strcmp(buffer, "165140"));

  sprintf(buffer, "%lo", 12345678L);
  REQUIRE(!strcmp(buffer, "57060516"));

  sprintf(buffer, "%lx", 0x12345678L);
  REQUIRE(!strcmp(buffer, "12345678"));

  sprintf(buffer, "%llx", 0x1234567891234567LLU);
  REQUIRE(!strcmp(buffer, "1234567891234567"));

  sprintf(buffer, "%lx", 0xabcdefabLU);
  REQUIRE(!strcmp(buffer, "abcdefab"));

  sprintf(buffer, "%lX", 0xabcdefabLU);
  REQUIRE(!strcmp(buffer, "ABCDEFAB"));

  sprintf(buffer, "%c", 'v');
  REQUIRE(!strcmp(buffer, "v"));

  sprintf(buffer, "%cv", 'w');
  REQUIRE(!strcmp(buffer, "wv"));

  sprintf(buffer, "%s", "A Test");
  REQUIRE(!strcmp(buffer, "A Test"));

  sprintf(buffer, "%hhu", 0xFFFFUL);
  REQUIRE(!strcmp(buffer, "255"));

  sprintf(buffer, "%hu", 0x123456UL);
  REQUIRE(!strcmp(buffer, "13398"));

  sprintf(buffer, "%s%hhi %hu", "Test", 10000, 0xFFFFFFFFU);
  REQUIRE(!strcmp(buffer, "Test16 65535"));

  sprintf(buffer, "%tx", &buffer[10] - &buffer[0]);
  REQUIRE(!strcmp(buffer, "a"));

// TBD
  if (sizeof(intmax_t) == sizeof(long)) {
    sprintf(buffer, "%ji", -2147483647L);
    REQUIRE(!strcmp(buffer, "-2147483647"));
  } else {
    sprintf(buffer, "%ji", -2147483647LL);
    REQUIRE(!strcmp(buffer, "-2147483647"));
  }
}


void test_pointer (void) {
  start("pointer");
  char buffer[100];

  sprintf(buffer, "%p", (void*)0x1234U);
  if (sizeof(void*) == 4U) {
    REQUIRE(!strcmp(buffer, "00001234"));
  }
  else {
    REQUIRE(!strcmp(buffer, "0000000000001234"));
  }

  sprintf(buffer, "%p", (void*)0x12345678U);
  if (sizeof(void*) == 4U) {
    REQUIRE(!strcmp(buffer, "12345678"));
  }
  else {
    REQUIRE(!strcmp(buffer, "0000000012345678"));
  }

  sprintf(buffer, "%p-%p", (void*)0x12345678U, (void*)0x7EDCBA98U);
  if (sizeof(void*) == 4U) {
    REQUIRE(!strcmp(buffer, "12345678-7EDCBA98"));
  }
  else {
    REQUIRE(!strcmp(buffer, "0000000012345678-000000007EDCBA98"));
  }

  if (sizeof(uintptr_t) == sizeof(uint64_t)) {
    sprintf(buffer, "%p", (void*)(uintptr_t)0xFFFFFFFFU);
    REQUIRE(!strcmp(buffer, "00000000FFFFFFFF"));
  }
  else {
    sprintf(buffer, "%p", (void*)(uintptr_t)0xFFFFFFFFU);
    REQUIRE(!strcmp(buffer, "FFFFFFFF"));
  }
}


void test_unknown_flag (void) {
  start("unknown_flag");
  char buffer[100];

  sprintf(buffer, "%kmarco", 42, 37);
  REQUIRE(!strcmp(buffer, "kmarco"));
}


void test_string_length (void) {
  start("string_length");
  char buffer[100];

  sprintf(buffer, "%.4s", "This is a test");
  REQUIRE(!strcmp(buffer, "This"));

  sprintf(buffer, "%.4s", "test");
  REQUIRE(!strcmp(buffer, "test"));

  sprintf(buffer, "%.7s", "123");
  REQUIRE(!strcmp(buffer, "123"));

  sprintf(buffer, "%.7s", "");
  REQUIRE(!strcmp(buffer, ""));

  sprintf(buffer, "%.4s%.2s", "123456", "abcdef");
  REQUIRE(!strcmp(buffer, "1234ab"));

  sprintf(buffer, "%.4.2s", "123456");
  REQUIRE(!strcmp(buffer, ".2s"));

  sprintf(buffer, "%.*s", 3, "123456");
  REQUIRE(!strcmp(buffer, "123"));
}


void test_buffer_length (void) {
  start("buffer_length");
  char buffer[100];
  int ret;

  ret = snprintf(NULL, 10, "%s", "Test");
  REQUIRE(ret == 4);
  ret = snprintf(NULL, 0, "%s", "Test");
  REQUIRE(ret == 4);

  buffer[0] = (char)0xA5;
  ret = snprintf(buffer, 0, "%s", "Test");
  REQUIRE(buffer[0] == (char)0xA5);
  REQUIRE(ret == 4);

  buffer[0] = (char)0xCC;
  snprintf(buffer, 1, "%s", "Test");
  REQUIRE(buffer[0] == '\0');

  snprintf(buffer, 2, "%s", "Hello");
  REQUIRE(!strcmp(buffer, "H"));
}


void test_ret_value (void) {
  start("ret_value");
  char buffer[100];
  int ret;

  ret = snprintf(buffer, 6, "0%s", "1234");
  REQUIRE(!strcmp(buffer, "01234"));
  REQUIRE(ret == 5);

  ret = snprintf(buffer, 6, "0%s", "12345");
  REQUIRE(!strcmp(buffer, "01234"));
  REQUIRE(ret == 6);  // '5' is truncated

  ret = snprintf(buffer, 6, "0%s", "1234567");
  REQUIRE(!strcmp(buffer, "01234"));
  REQUIRE(ret == 8);  // '567' are truncated

  ret = snprintf(buffer, 10, "hello, world");
  REQUIRE(ret == 12);

  ret = snprintf(buffer, 3, "%d", 10000);
  REQUIRE(ret == 5);
  REQUIRE(strlen(buffer) == 2U);
  REQUIRE(buffer[0] == '1');
  REQUIRE(buffer[1] == '0');
  REQUIRE(buffer[2] == '\0');
}


void test_misc (void) {
  start("misc");
  char buffer[100];

  sprintf(buffer, "%u%u%ctest%d %s", 5, 3000, 'a', -20, "bit");
  REQUIRE(!strcmp(buffer, "53000atest-20 bit"));

  sprintf(buffer, "%.*f", 2, 0.33333333);
  REQUIRE(!strcmp(buffer, "0.33"));

  sprintf(buffer, "%.*d", -1, 1);
  REQUIRE(!strcmp(buffer, "1"));

  sprintf(buffer, "%.3s", "foobar");
  REQUIRE(!strcmp(buffer, "foo"));

  sprintf(buffer, "% .0d", 0);
  REQUIRE(!strcmp(buffer, " "));

  sprintf(buffer, "%10.5d", 4);
  REQUIRE(!strcmp(buffer, "     00004"));

  sprintf(buffer, "%*sx", -3, "hi");
  REQUIRE(!strcmp(buffer, "hi x"));

  sprintf(buffer, "%.*g", 2, 0.33333333);
  REQUIRE(!strcmp(buffer, "0.33"));

  sprintf(buffer, "%.*e", 2, 0.33333333);
  REQUIRE(!strcmp(buffer, "3.33e-01"));
}


/* scanf tests */

void test_standard_Pd(void) {
  start("standard %d");
  char *buffer = "42";
  int x0;
  int mc, cc;
  mc = sscanf("42", "%d%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 2);
}

void test_standard_Pd_with_preceding_whitespace(void) {
  start("standard %d with preceding whitespace");
  char *buffer = "  42";
  int x0;
  int mc, cc;
  mc = sscanf("  42", "%d%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 4);
}

void test_standard_Pd_with_preceding_newline(void) {
  start("standard %d with preceding newline");
  char *buffer = "\n42";
  int x0;
  int mc, cc;
  mc = sscanf("\n42", "%d%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 3);
}

void test_negative_Pd(void) {
  start("negative %d");
  char *buffer = "-42";
  int x0;
  int mc, cc;
  mc = sscanf("-42", "%d%n", &x0, &cc);
  //printf("%d,%d,%d\n", mc, x0, cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == -42);
  REQUIRE(cc == 3);
}

void test_PdPc_without_any_whitespace(void) {
  start("%d%c without any whitespace");
  char *buffer = "64c";
  int x0;
  char x1;
  int mc, cc;
  mc = sscanf("64c", "%d%c%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == 64);
  REQUIRE(x1 == 'c');
  REQUIRE(cc == 3);
}

void test_PdPc_with_whitespace_in_text_but_not_in_format(void) {
  start("%d%c with whitespace in text but not in format");
  char *buffer = "64 c";
  int x0;
  char x1;
  int mc, cc;
  mc = sscanf("64 c", "%d%c%n", &x0, &x1, &cc);
  //printf("%d,%d,%d,%d\n", mc, x0, x1, cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == 64);
  REQUIRE(x1 == ' ');
  REQUIRE(cc == 3);
}

void test_PdPc_with_whitespace_in_format_but_not_in_text(void) {
  start("%d%c with whitespace in format but not in text");
  char *buffer = "64c";
  int x0;
  char x1;
  int mc, cc;
  mc = sscanf("64c", "%d %c%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == 64);
  REQUIRE(x1 == 'c');
  REQUIRE(cc == 3);
}

void test_PdPc_with_whitespace_in_both(void) {
  start("%d%c with whitespace in both");
  char *buffer = "64 c";
  int x0;
  char x1;
  int mc, cc;
  mc = sscanf("64 c", "%d %c%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == 64);
  REQUIRE(x1 == 'c');
  REQUIRE(cc == 4);
}

void test_early_EOF(void) {
  start("early EOF");
  char *buffer = "";
  int x0;
  int mc, cc;
  mc = sscanf("", "%d%n", &x0, &cc);
  REQUIRE(mc == EOF);
}

void test_exact_char_match_1(void) {
  start("exact char match 1");
  char *buffer = "abc9";
  int x0;
  int mc, cc;
  mc = sscanf("abc9", "abc%d%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 9);
  REQUIRE(cc == 4);
}

void test_exact_char_match_0(void) {
  start("exact char match 0");
  char *buffer = "abd9";
  int x0 = 42;
  int mc, cc = 53;
  mc = sscanf("abd9", "abc%d%n", &x0, &cc);
  //printf("%d,%d,%d\n", mc, x0, cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 53);
}

void test_standard_Pu(void) {
  start("standard %u");
  char *buffer = "42";
  unsigned x0;
  int mc, cc;
  mc = sscanf("42", "%u%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(cc == 2);
}

void test_decimal_Pi(void) {
  start("decimal %i");
  char *buffer = "42";
  int x0;
  int mc, cc;
  mc = sscanf("42", "%i%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 2);
}

void test_hexadecimal_Pi_lowercase(void) {
  start("hexadecimal %i lowercase");
  char *buffer = "0x2a";
  int x0;
  int mc, cc;
  mc = sscanf("0x2a", "%i%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 4);
}

void test_hexadecimal_Pi_uppercase(void) {
  start("hexadecimal %i uppercase");
  char *buffer = "0X2A";
  int x0;
  int mc, cc;
  mc = sscanf("0X2A", "%i%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 4);
}

void test_octal_Pi(void) {
  start("octal %i");
  char *buffer = "052";
  int x0;
  int mc, cc;
  mc = sscanf("052", "%i%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 3);
}

void test_hexadecimal_Px_lowercase(void) {
  start("hexadecimal %x lowercase");
  char *buffer = "2a";
  int x0;
  int mc, cc;
  mc = sscanf("2a", "%x%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 2);
}

void test_hexadecimal_Px_uppercase(void) {
  start("hexadecimal %x uppercase");
  char *buffer = "2A";
  int x0;
  int mc, cc;
  mc = sscanf("2A", "%x%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 2);
}

void test_hexadecimal_Px_0x_lowercase(void) {
  start("hexadecimal %x 0x lowercase");
  char *buffer = "0x2a";
  int x0;
  int mc, cc;
  mc = sscanf("0x2a", "%x%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 4);
}

void test_hexadecimal_Px_0x_uppercase(void) {
  start("hexadecimal %x 0x uppercase");
  char *buffer = "0X2A";
  int x0;
  int mc, cc;
  mc = sscanf("0X2A", "%x%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 4);
}

void test_octal_Po(void) {
  start("octal %o");
  char *buffer = "52";
  int x0;
  int mc, cc;
  mc = sscanf("52", "%o%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 2);
}

void test_hexadecimal_edge_g(void) {
  start("hexadecimal edge g");
  char *buffer = "0x2ag";
  int x0;
  char x1;
  int mc, cc;
  mc = sscanf("0x2ag", "%x%c%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == 42);
  REQUIRE(x1 == 'g');
  REQUIRE(cc == 5);
}

void test_Pd_with_no_valid_chars_1(void) {
  start("%d with no valid chars 1");
  char *buffer = "e";
  int x0 = 42;
  int mc, cc;
  mc = sscanf("e", "%d%n", &x0, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 42);
}

void test_Pd_with_no_valid_chars_2(void) {
  start("%d with no valid chars 2");
  char *buffer = "abc";
  int x0 = 42;
  int mc, cc;
  mc = sscanf("abc", "%d%n", &x0, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 42);
}

void test_Pd_max_width(void) {
  start("%d max width");
  char *buffer = "1234";
  int x0;
  int x1;
  int mc, cc;
  mc = sscanf("1234", "%2d%2d%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == 12);
  REQUIRE(x1 == 34);
  REQUIRE(cc == 4);
}

void test_PSd(void) {
  start("%*d");
  char *buffer = "1 2";
  int x0;
  int mc, cc;
  mc = sscanf("1 2", "%*d%d%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 2);
  REQUIRE(cc == 3);
}

void test_Pd_with_no_valid_chars_other_than_sign(void) {
  start("%d with no valid chars other than sign");
  char *buffer = "+e";
  int x0 = 42;
  int mc, cc = 53;
  mc = sscanf("+e", "%d%n", &x0, &cc);
  //printf("%d,%d,%d\n", mc, x0, cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 53);
}

void test_Pd_with_no_valid_chars_followed_by_matching(void) {
  start("%d with no valid chars followed by matching");
  char *buffer = "+e";
  int x0 = 42;
  char x1 = '?';
  int mc, cc;
  mc = sscanf("+e", "%d%c%n", &x0, &x1, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 42);
  REQUIRE(x1 == '?');
}

void test_Pd_zero(void) {
  start("%d zero");
  char *buffer = "0";
  int x0;
  int mc, cc;
  mc = sscanf("0", "%d%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 0);
  REQUIRE(cc == 1);
}

void test_Pi_zero(void) {
  start("%i zero");
  char *buffer = "0";
  int x0;
  int mc, cc;
  mc = sscanf("0", "%i%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 0);
  REQUIRE(cc == 1);
}

void test_Px_zero(void) {
  start("%x zero");
  char *buffer = "0";
  int x0;
  int mc, cc;
  mc = sscanf("0", "%x%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 0);
  REQUIRE(cc == 1);
}

void test_Pd_multizero(void) {
  start("%d multizero");
  char *buffer = "0000";
  int x0;
  int mc, cc;
  mc = sscanf("0000", "%d%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 0);
  REQUIRE(cc == 4);
}

void test_Pi_0x_with_garbage(void) {
  start("%i 0x with garbage");
  char *buffer = "0xg";
  int x0 = 42;
  int mc, cc = 53;
  mc = sscanf("0xg", "%i%n", &x0, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 53);
}

void test_Px_0x_with_garbage(void) {
  start("%x 0x with garbage");
  char *buffer = "0xg";
  int x0 = 42;
  int mc, cc = 53;
  mc = sscanf("0xg", "%x%n", &x0, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 53);
}

void test_Pi_failure_mode(void) {
  start("%i failure mode");
  char *buffer = "09";
  int x0;
  int x1;
  int mc, cc;
  mc = sscanf("09", "%i%d%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == 0);
  REQUIRE(x1 == 9);
  REQUIRE(cc == 2);
}

void test_Pc(void) {
  start("%c");
  char *buffer = "abc";
  char x0;
  char x1;
  char x2;
  int mc, cc;
  mc = sscanf("abc", "%c%c%c%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 3);
  REQUIRE(x0 == 'a');
  REQUIRE(x1 == 'b');
  REQUIRE(x2 == 'c');
  REQUIRE(cc == 3);
}

void test_P2c(void) {
  start("%2c");
  char *buffer = "abc";
  char x0[2];
  int mc, cc;
  mc = sscanf("abc", "%2c%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0[0] == 'a' && x0[1] == 'b');
  REQUIRE(cc == 2);
}

void test_P2cPc(void) {
  start("%2c%c");
  char *buffer = "abc";
  char x0[2];
  char x1;
  int mc, cc;
  mc = sscanf("abc", "%2c%c%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(x0[0] == 'a' && x0[1] == 'b');
  REQUIRE(x1 == 'c');
  REQUIRE(cc == 3);
}

void test_PSc(void) {
  start("%*c");
  char *buffer = "ab";
  char x0;
  int mc, cc;
  mc = sscanf("ab", "%*c%c%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 'b');
  REQUIRE(cc == 2);
}

void test_P3c(void) {
  start("%3c");
  char *buffer = "ab\ncd";
  char x0[3];
  int mc, cc;
  mc = sscanf("ab\ncd", "%3c%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0[0] == 'a' && x0[1] == 'b' && x0[2] == '\n');
  REQUIRE(cc == 3);
}

void test_Ps(void) {
  start("%s");
  char *buffer = "abc";
  char x0[20];
  int mc, cc;
  mc = sscanf("abc", "%s%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(cc == 3);
}

void test_Ps_preceding_whitespace(void) {
  start("%s preceding whitespace");
  char *buffer = "  abc";
  char x0[20];
  int mc, cc;
  mc = sscanf("  abc", "%s%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(cc == 5);
}

void test_Ps_preceding_whitespace_2(void) {
  start("%s preceding whitespace 2");
  char *buffer = "  abc  def";
  char x0[20];
  char x1[20];
  int mc, cc;
  mc = sscanf("  abc  def", "%s%s%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(!strcmp(x1, "def"));
  REQUIRE(cc == 10);
}

void test_Ps_preceding_whitespace_3(void) {
  start("%s preceding whitespace 3");
  char *buffer = "abc  def";
  char x0[20];
  char x1[20];
  int mc, cc;
  mc = sscanf("abc  def", "%s%s%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(!strcmp(x1, "def"));
  REQUIRE(cc == 8);
}

void test_P5s(void) {
  start("%5s");
  char *buffer = "abcdef   ghi";
  char x0[20];
  char x1[20];
  int mc, cc;
  mc = sscanf("abcdef   ghi", "%5s%s%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(!strcmp(x0, "abcde"));
  REQUIRE(!strcmp(x1, "f"));
  REQUIRE(cc == 6);
}

void test_scanset_found(void) {
  start("scanset found");
  char *buffer = "abc";
  char x0[20];
  int mc, cc;
  mc = sscanf("abc", "%[abc]%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(cc == 3);
}

void test_scanset_found_rev(void) {
  start("scanset found rev");
  char *buffer = "abc";
  char x0[20];
  int mc, cc;
  mc = sscanf("abc", "%[bca]%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(cc == 3);
}

void test_scanset_found_with_two_sets(void) {
  start("scanset found with two sets");
  char *buffer = "abcde";
  char x0[20];
  char x1[20];
  int mc, cc;
  mc = sscanf("abcde", "%[abc]%[de]%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(!strcmp(x1, "de"));
  REQUIRE(cc == 5);
}

void test_inverted_scansets(void) {
  start("inverted scansets");
  char *buffer = "bbba";
  char x0[20];
  int mc, cc;
  mc = sscanf("bbba", "%[^a]%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "bbb"));
  REQUIRE(cc == 3);
}

void test_scansets_combined(void) {
  start("scansets combined");
  char *buffer = "abbadededeabba";
  char x0[20];
  char x1[20];
  char x2[20];
  int mc, cc;
  mc = sscanf("abbadededeabba", "%[ab]%[^ab]%[ab]%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 3);
  REQUIRE(!strcmp(x0, "abba"));
  REQUIRE(!strcmp(x1, "dedede"));
  REQUIRE(!strcmp(x2, "abba"));
  REQUIRE(cc == 14);
}

void test_scansets_starting_with_hyphen(void) {
  start("scansets starting with hyphen");
  char *buffer = "-a0";
  char x0[20];
  int mc, cc;
  mc = sscanf("-a0", "%[-a]%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "-a"));
  REQUIRE(cc == 2);
}

void test_scansets_ending_with_hyphen(void) {
  start("scansets ending with hyphen");
  char *buffer = "-a0";
  char x0[20];
  int mc, cc;
  mc = sscanf("-a0", "%[a-]%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "-a"));
  REQUIRE(cc == 2);
}

void test_inverted_scansets_starting_with_hyphen(void) {
  start("inverted scansets starting with hyphen");
  char *buffer = "2b-a0";
  char x0[20];
  int mc, cc;
  mc = sscanf("2b-a0", "%[^-a]%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "2b"));
  REQUIRE(cc == 2);
}

void test_inverted_scansets_ending_with_hyphen(void) {
  start("inverted scansets ending with hyphen");
  char *buffer = "2b-a0";
  char x0[20];
  int mc, cc;
  mc = sscanf("2b-a0", "%[^a-]%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "2b"));
  REQUIRE(cc == 2);
}

void test_scanset_range(void) {
  start("scanset range");
  char *buffer = "01239a";
  char x0[20];
  int mc, cc;
  mc = sscanf("01239a", "%[0-9]a%n", &x0, &cc);
  //printf("%d,[%s],%d\n", mc, &x0[0], cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "01239"));
  REQUIRE(cc == 6);
}

void test_scanset_range_one_char(void) {
  start("scanset range one char");
  char *buffer = "555a";
  char x0[20];
  int mc, cc;
  mc = sscanf("555a", "%[5-5]a%n", &x0, &cc);
  //printf("%d,[%s],%d\n", mc, &x0[0], cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "555"));
  REQUIRE(cc == 4);
}

void test_inverted_scanset_range(void) {
  start("inverted scanset range");
  char *buffer = "abc0def";
  char x0[20];
  int mc, cc;
  mc = sscanf("abc0def", "%[^0-9]%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(cc == 3);
}

void test_scanset_maxlen(void) {
  start("scanset maxlen");
  char *buffer = "39abc";
  char x0[20];
  char x1[20];
  int mc, cc;
  mc = sscanf("39abc", "%2[0-9]%2[a-b]%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(!strcmp(x0, "39"));
  REQUIRE(!strcmp(x1, "ab"));
  REQUIRE(cc == 4);
}

void test_Pf(void) {
  start("%f");
  char *buffer = "2.5";
  float x0;
  int mc, cc;
  mc = sscanf("2.5", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 2.5f);
  REQUIRE(cc == 3);
}

void test_Pe(void) {
  start("%e");
  char *buffer = "2.5";
  float x0;
  int mc, cc;
  mc = sscanf("2.5", "%e%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 2.5f);
  REQUIRE(cc == 3);
}

void test_Pg(void) {
  start("%g");
  char *buffer = "2.5";
  float x0;
  int mc, cc;
  mc = sscanf("2.5", "%g%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 2.5f);
  REQUIRE(cc == 3);
}

void test_Pa(void) {
  start("%a");
  char *buffer = "2.5";
  float x0;
  int mc, cc;
  mc = sscanf("2.5", "%a%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 2.5f);
  REQUIRE(cc == 3);
}

void test_Pf_multidots_1(void) {
  start("%f multidots 1");
  char *buffer = "2.5.2";
  float x0;
  int mc, cc;
  mc = sscanf("2.5.2", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 2.5f);
  REQUIRE(cc == 3);
}

void test_Pf_multidots_2(void) {
  start("%f multidots 2");
  char *buffer = "2..2";
  float x0;
  int mc, cc;
  mc = sscanf("2..2", "%f%n", &x0, &cc);
  //printf("%d,%f,%d\n", mc, (double)x0, cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 2.0f);
  REQUIRE(cc == 2);
}

void test_Pf_0_5 (void) {
  start("%f 0.5");
  char *buffer = "0.5";
  float x0;
  int mc, cc;
  mc = sscanf("0.5", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 0.5f);
  REQUIRE(cc == 3);
}

void test_Pf_startdot(void) {
  start("%f startdot");
  char *buffer = ".2";
  float x0;
  int mc, cc;
  mc = sscanf(".2", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 0.2f);
  REQUIRE(cc == 2);
}

void test_Pf_dot_only(void) {
  start("%f dot only");
  char *buffer = ".a";
  float x0 = 4.2f;
  int mc, cc = 53;
  mc = sscanf(".a", "%f%n", &x0, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 4.2f);
  REQUIRE(cc == 53);
}

void test_Pf_plus(void) {
  start("%f plus");
  char *buffer = "+2.5";
  float x0;
  int mc, cc;
  mc = sscanf("+2.5", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 2.5f);
  REQUIRE(cc == 4);
}

void test_Pf_minus(void) {
  start("%f minus");
  char *buffer = "-2.5";
  float x0;
  int mc, cc;
  mc = sscanf("-2.5", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == -2.5f);
  REQUIRE(cc == 4);
}

void test_Pf_plus_dot(void) {
  start("%f plus dot");
  char *buffer = "+.5";
  float x0;
  int mc, cc;
  mc = sscanf("+.5", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 0.5f);
  REQUIRE(cc == 3);
}

void test_Pf_enddot(void) {
  start("%f enddot");
  char *buffer = "-2.a";
  float x0;
  char x1;
  int mc, cc;
  mc = sscanf("-2.a", "%f%c%n", &x0, &x1, &cc);
  //printf("%d,%f,'%c',%d\n", mc, (double)x0, x1, cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == -2.0f);
  REQUIRE(x1 == 'a');
  REQUIRE(cc == 4);
}

void test_Pf_pi(void) {
  start("%f pi");
  char *buffer = "3.14159";
  float x0;
  int mc, cc;
  mc = sscanf("3.14159", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 3.14159f);
  REQUIRE(cc == 7);
}

void test_Pf_exp_plus(void) {
  start("%f exp+");
  char *buffer = "100.5e+3";
  float x0;
  int mc, cc;
  mc = sscanf("100.5e+3", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 100500.0f);
  REQUIRE(cc == 8);
}

void test_Pf_exp_minus(void) {
  start("%f exp-");
  char *buffer = "12.5e-2";
  float x0;
  int mc, cc;
  mc = sscanf("12.5e-2", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 0.125f);
  REQUIRE(cc == 7);
}

void test_Pf_hex(void) {
  start("%f hex");
  char *buffer = "0x0.3p10";
  float x0;
  int mc, cc;
  mc = sscanf("0x0.3p10", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 192.0f);
  REQUIRE(cc == 8);
}

void test_Pf_inf(void) {
  start("%f inf");
  char *buffer = "inf";
  float x0;
  int mc, cc;
  mc = sscanf("inf", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == INFINITY);
  REQUIRE(cc == 3);
}

void test_Pf_minus_infinity(void) {
  start("%f -infinity");
  char *buffer = "-INFINITY";
  float x0;
  int mc, cc;
  mc = sscanf("-INFINITY", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == -INFINITY);
  REQUIRE(cc == 9);
}

void test_Pf_nan(void) {
  start("%f nan");
  char *buffer = "nan";
  float x0;
  int mc, cc;
  mc = sscanf("nan", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 != x0);
  REQUIRE(cc == 3);
}

void test_Pf_nan_with_msg(void) {
  start("%f nan with msg");
  char *buffer = "nan(0123)";
  float x0;
  int mc, cc;
  mc = sscanf("nan(0123)", "%f%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 != x0);
  REQUIRE(cc == 9);
}

void test_Pp(void) {
  start("%p");
  char *buffer = "0x01ff";
  void *x0;
  int mc, cc;
  mc = sscanf("0x01ff", "%p%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(cc == 6);
}

/*void test_Pp_null(void) {
  start("%p (null)");
  char *buffer = "(null)";
  void *x0 = buffer;
  int mc, cc;
  mc = sscanf("(null)", "%p%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == NULL);
  REQUIRE(cc == 5);
}*/

/*void test_Pb(void) {
  start("%b");
  char *buffer = "1001101001";
  int x0;
  int mc, cc;
  mc = sscanf("1001101001", "%b%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 617);
  REQUIRE(cc == 10);
}*/

void test_literal(void) {
  start("literal");
  char *buffer = "abc3";
  int x0;
  int mc, cc;
  mc = sscanf("abc3", "abc%d%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 3);
  REQUIRE(cc == 4);
}

void test_literal_with_whitespace_on_input_only(void) {
  start("literal with whitespace on input only");
  char *buffer = " abc3";
  int x0 = 42;
  int mc, cc = 53;
  mc = sscanf(" abc3", "abc%d%n", &x0, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 42);
  REQUIRE(cc == 53);
}

void test_literal_with_whitespace_on_both(void) {
  start("literal with whitespace on both");
  char *buffer = " abc3";
  int x0;
  int mc, cc;
  mc = sscanf(" abc3", " abc%d%n", &x0, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 3);
  REQUIRE(cc == 5);
}

void test_hours_minutes_seconds(void) {
  start("hours:minutes:seconds");
  char *buffer = "02:50:09";
  int x0;
  int x1;
  int x2;
  int mc, cc;
  mc = sscanf("02:50:09", "%d:%d:%d%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 3);
  REQUIRE(x0 == 2);
  REQUIRE(x1 == 50);
  REQUIRE(x2 == 9);
  REQUIRE(cc == 8);
}

void test_whitespace_test_1(void) {
  start("whitespace test 1");
  char *buffer = "abc de 123";
  char x0[3];
  char x1[3];
  int x2 = 42;
  int mc, cc = 53;
  mc = sscanf("abc de 123", "%3c%3c%d%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 3);
  REQUIRE(x0[0] == 'a' && x0[1] == 'b' && x0[2] == 'c');
  REQUIRE(x1[0] == ' ' && x1[1] == 'd' && x1[2] == 'e');
  REQUIRE(x2 == 123);
  REQUIRE(cc == 10);
}

void test_whitespace_test_2(void) {
  start("whitespace test 2");
  char *buffer = "a b";
  char x0;
  char x1;
  int mc, cc;
  mc = sscanf("a b", "%c\n\n%c%n", &x0, &x1, &cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == 'a');
  REQUIRE(x1 == 'b');
  REQUIRE(cc == 3);
}

void test_Pn_on_empty(void) {
  start("%n on empty");
  char *buffer = "";
  int x0;
  int mc, cc;
  mc = sscanf("", "%n%n", &x0, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 0);
  REQUIRE(cc == 0);
}

void test_Pn_0(void) {
  start("%n 0");
  char *buffer = "a";
  int x0;
  int mc, cc;
  mc = sscanf("a", "%n%n", &x0, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 0);
  REQUIRE(cc == 0);
}

void test_Pn_1_N1(void) {
  start("%n 1 #1");
  char *buffer = "a";
  char x0;
  int x1;
  int mc, cc;
  mc = sscanf("a", "%c%n%n", &x0, &x1, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 'a');
  REQUIRE(x1 == 1);
  REQUIRE(cc == 1);
}

void test_Pn_1_N2(void) {
  start("%n 1 #2");
  char *buffer = "ab";
  char x0;
  int x1;
  char x2;
  int mc, cc;
  mc = sscanf("ab", "%c%n%c%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == 'a');
  REQUIRE(x1 == 1);
  REQUIRE(x2 == 'b');
  REQUIRE(cc == 2);
}

void test_Pn_after_Ps(void) {
  start("%n after %s");
  char *buffer = "abc d";
  char x0[3+1];
  int x1;
  int mc, cc;
  mc = sscanf("abc d", "%s%n%n", &x0, &x1, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0[0] == 'a' && x0[1] == 'b' && x0[2] == 'c');
  REQUIRE(x1 == 3);
  REQUIRE(cc == 3);
}

void test_Pn_after_Ps_and_whitespace(void) {
  start("%n after %s and whitespace");
  char *buffer = "  abcd";
  char x0[3+1];
  int x1;
  int mc, cc;
  mc = sscanf("  abcd", "%3s%n%n", &x0, &x1, &cc);
  REQUIRE(mc == 1);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(x1 == 5);
  REQUIRE(cc == 5);
}

void test_Pn_after_Ps_with_trailing_whitespace(void) {
  start("%n after %s with trailing whitespace");
  char *buffer = "  abc      2bb";
  char x0[3+1];
  int x1;
  int x2;
  int x3;
  int mc, cc;
  mc = sscanf("  abc      2bb", "%s%n%d%n%n", &x0, &x1, &x2, &x3, &cc);
  REQUIRE(mc == 2);
  REQUIRE(!strcmp(x0, "abc"));
  REQUIRE(x1 == 5);
  REQUIRE(x2 == 2);
  REQUIRE(x3 == 12);
  REQUIRE(cc == 12);
}

void test_Pn_after_Pf(void) {
  start("%n after %f");
  char *buffer = "-75e-2q";
  float x0;
  int x1;
  int mc, cc;
  mc = sscanf("-75e-2q", "%f%n%n", &x0, &x1, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == -0.75f);
  REQUIRE(x1 == 6);
  REQUIRE(cc == 6);
}

void test_C99_example_1(void) {
  start("C99 example 1");
  char *buffer = "25 54.32E-1 thompson";
  int x0;
  float x1;
  char x2[50];
  int mc, cc;
  mc = sscanf("25 54.32E-1 thompson", "%d%f%s%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 3);
  REQUIRE(x0 == 25);
  REQUIRE(x1 == 5.432f);
  REQUIRE(!strcmp(x2, "thompson"));
  REQUIRE(cc == 20);
}

void test_C99_example_2(void) {
  start("C99 example 2");
  char *buffer = "56789 0123 56a72";
  int x0;
  float x1;
  char x2[50];
  int mc, cc;
  mc = sscanf("56789 0123 56a72", "%2d%f%*d %[0123456789]%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 3);
  REQUIRE(x0 == 56);
  REQUIRE(x1 == 789);
  REQUIRE(!strcmp(x2, "56"));
  REQUIRE(cc == 13);
}

void test_C99_example_3a(void) {
  start("C99 example 3a");
  char *buffer = "-12.8degrees Celsius";
  float x0;
  char x1[21];
  char x2[21];
  int mc, cc = 53;
  mc = sscanf("-12.5degrees Celsius", "%f%20s of %20s%n", &x0, &x1, &x2, &cc);
  //printf("%d,%f,[%s],%d\n", mc, (double)x0, &x1[0], cc);
  REQUIRE(mc == 2);
  REQUIRE(x0 == -12.5);
  REQUIRE(!strcmp(x1, "degrees"));
  REQUIRE(cc == 53);
}

void test_C99_example_3b(void) {
  start("C99 example 3b");
  char *buffer = "lots of luck";
  float x0 = 4.2f;
  char x1[21];
  char x2[21];
  int mc, cc = 53;
  mc = sscanf("lots of luck", "%f%20s of %20s%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 4.2f);
  REQUIRE(cc == 53);
}

void test_C99_example_3c(void) {
  start("C99 example 3c");
  char *buffer = "10.0LBS\t of \t fertilizer";
  float x0;
  char x1[21];
  char x2[21];
  int mc, cc;
  mc = sscanf("10.0LBS\t of \t fertilizer", "%f%20s of %20s%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 3);
  REQUIRE(x0 == 10.0);
  REQUIRE(!strcmp(x1, "LBS"));
  REQUIRE(!strcmp(x2, "fertilizer"));
  REQUIRE(cc == 24);
}

void test_C99_example_3d(void) {
  start("C99 example 3d");
  char *buffer = "100ergs of energy";
  float x0 = 4.2f;
  char x1[21];
  char x2[21];
  int mc, cc = 53;
  mc = sscanf("100ergs of energy", "%f%20s of %20s%n", &x0, &x1, &x2, &cc);
  REQUIRE(mc == 0);
  REQUIRE(x0 == 4.2f);
  REQUIRE(cc == 53);
}

void test_C99_example_4(void) {
  start("C99 example 4");
  char *buffer = "123";
  int x0;
  int x1;
  int x2;
  int x3 = 42;
  int mc, cc = 53;
  mc = sscanf("123", "%d%n%n%d%n", &x0, &x1, &x2, &x3, &cc);
  REQUIRE(mc == 1);
  REQUIRE(x0 == 123);
  REQUIRE(x1 == 3);
  REQUIRE(x2 == 3);
  REQUIRE(x3 == 42);
  REQUIRE(cc == 53);
}


int main()
{
  /* printf tests */
  test_snprintf();
  //test_vprintf();
  //test_vsnprintf();
  test_space_flag();
  test_plus_flag();
  test_0_flag();
  test_minus_flag();
  test_hash_flag();
  test_specifier();
  test_width();
  test_width_20();
  test_width_star_20();
  test_width_minus_20();
  test_width_0_20();
  test_padding_20();
  test_padding_dot_20();
  test_padding_hash_020();
  test_padding_hash_20();
  test_padding_20_dot_5();
  test_padding_neg_numbers();
  test_float_padding_neg_numbers();
  test_length();
  test_float();
  test_float_hex();
  test_types();
  test_pointer();
  test_unknown_flag();
  test_string_length();
  test_buffer_length();
  test_ret_value();
  test_misc();

  /* scanf tests */
  test_standard_Pd();
  test_standard_Pd_with_preceding_whitespace();
  test_standard_Pd_with_preceding_newline();
  test_negative_Pd();
  test_PdPc_without_any_whitespace();
  test_PdPc_with_whitespace_in_text_but_not_in_format();
  test_PdPc_with_whitespace_in_format_but_not_in_text();
  test_PdPc_with_whitespace_in_both();
  test_early_EOF();
  test_exact_char_match_1();
  test_exact_char_match_0();
  test_standard_Pu();
  test_decimal_Pi();
  test_hexadecimal_Pi_lowercase();
  test_hexadecimal_Pi_uppercase();
  test_octal_Pi();
  test_hexadecimal_Px_lowercase();
  test_hexadecimal_Px_uppercase();
  test_hexadecimal_Px_0x_lowercase();
  test_hexadecimal_Px_0x_uppercase();
  test_octal_Po();
  test_hexadecimal_edge_g();
  test_Pd_with_no_valid_chars_1();
  test_Pd_with_no_valid_chars_2();
  test_Pd_max_width();
  test_PSd();
  test_Pd_with_no_valid_chars_other_than_sign();
  test_Pd_with_no_valid_chars_followed_by_matching();
  test_Pd_zero();
  test_Pi_zero();
  test_Px_zero();
  test_Pd_multizero();
  test_Pi_0x_with_garbage();
  test_Px_0x_with_garbage();
  test_Pi_failure_mode();
  test_Pc();
  test_P2c();
  test_P2cPc();
  test_PSc();
  test_P3c();
  test_Ps();
  test_Ps_preceding_whitespace();
  test_Ps_preceding_whitespace_2();
  test_Ps_preceding_whitespace_3();
  test_P5s();
  test_scanset_found();
  test_scanset_found_rev();
  test_scanset_found_with_two_sets();
  test_inverted_scansets();
  test_scansets_combined();
  test_scansets_starting_with_hyphen();
  test_scansets_ending_with_hyphen();
  test_inverted_scansets_starting_with_hyphen();
  test_inverted_scansets_ending_with_hyphen();
  test_scanset_range();
  test_scanset_range_one_char();
  test_inverted_scanset_range();
  test_scanset_maxlen();
  test_Pf();
  test_Pe();
  test_Pg();
  test_Pa();
  test_Pf_multidots_1();
  test_Pf_multidots_2();
  test_Pf_0_5();
  test_Pf_startdot();
  test_Pf_dot_only();
  test_Pf_plus();
  test_Pf_minus();
  test_Pf_plus_dot();
  test_Pf_enddot();
  test_Pf_pi();
  test_Pf_exp_plus();
  test_Pf_exp_minus();
  test_Pf_hex();
  test_Pf_inf();
  test_Pf_minus_infinity();
  test_Pf_nan();
  test_Pf_nan_with_msg();
  test_Pp();
  //test_Pp_null();
  //test_Pb();
  test_literal();
  test_literal_with_whitespace_on_input_only();
  test_literal_with_whitespace_on_both();
  test_hours_minutes_seconds();
  test_whitespace_test_1();
  test_whitespace_test_2();
  test_Pn_on_empty();
  test_Pn_0();
  test_Pn_1_N1();
  test_Pn_1_N2();
  test_Pn_after_Ps();
  test_Pn_after_Ps_and_whitespace();
  test_Pn_after_Ps_with_trailing_whitespace();
  test_Pn_after_Pf();
  test_C99_example_1();
  test_C99_example_2();
  test_C99_example_3a();
  test_C99_example_3b();
  test_C99_example_3c();
  test_C99_example_3d();
  test_C99_example_4();
  
  printf("\nFAILURES: %d\n", failcnt);
  return failcnt;
}
