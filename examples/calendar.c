/* C program to print the month by month
 * calendar for the given year */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
int dayNumber(int day, int month, int year) 
{
  year -= month < 3;
  return (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
}

char *getMonthName(int monthNumber) 
{
  char *month;
  switch (monthNumber) {
    case 0:  month = "January";   break;
    case 1:  month = "February";  break;
    case 2:  month = "March";     break;
    case 3:  month = "April";     break;
    case 4:  month = "May";       break;
    case 5:  month = "June";      break;
    case 6:  month = "July";      break;
    case 7:  month = "August";    break;
    case 8:  month = "September"; break;
    case 9:  month = "October";   break;
    case 10: month = "November";  break;
    case 11: month = "December";  break;
  }
  return month;
}

int numberOfDays(int monthNumber, int year) 
{
  switch (monthNumber) {
    case 0:  return 31;
    case 1:  return (year % 400 == 0 || (year % 4 == 0 && year % 100 != 0)) ? 29 : 28;
    case 3:  return 30;
    case 5:  return 30;
    case 8:  return 30;
    case 10: return 30;
  }
  return 31;
}

void printCalendar(int year) 
{
  printf("\n                %d\n\n", year);
  int i, j, k;
  int current = dayNumber(1, 1, year);
  for (i = 0; i < 12; i++) {
    char *mn = getMonthName(i); int len = (int)strlen(mn);
    int lc = (33-len)/2, rc = (33-len)-lc;
    int days = numberOfDays(i, year);
    printf("\n  %.*s%s%.*s\n", lc, "------------------", mn, rc, "------------------");
    printf("  Sun  Mon  Tue  Wed  Thu  Fri  Sat\n");
    for (k = 0; k < current; k++)
      printf("     ");
    for (j = 1; j <= days; j++) {
      printf("%5d", j);
      if (++k > 6) {
        k = 0;
        printf("\n");
      }
    }
    if (k)
      printf("\n");
    current = k;
  }
}

int main(int argc, char **argv) 
{
  int year;
  if (argc < 2 || (year = atoi(argv[1])) <= 0) {
    printf("This program prints calendar for a given year\n"
           "usage: %s year\n", argv[0]);
    return 1;
  }
  printCalendar(year);
  return 0;
}

