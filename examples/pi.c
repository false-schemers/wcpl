/*
 * Computation of the n'th decimal digit of \pi with very little memory.
 * Written by Fabrice Bellard on January 8, 1997.
 *
 * We use a slightly modified version of the method described by Simon
 * Plouffe in "On the Computation of the n'th decimal digit of various
 * transcendental numbers" (November 1996). We have modified the algorithm
 * to get a running time of O(n^2) instead of O(n^3log(n)^3).
 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define mul_mod(a, b, m) ((int)(((long long)(a) * (long long)(b)) % (m)))

/* return the inverse of x mod y */
static int inv_mod(int x, int y) 
{
  int q, u, v, a, c, t;
  u = x;
  v = y;
  c = 1;
  a = 0;
  do {
    q = v / u;
    t = c;
    c = a - q * c;
    a = t;
    t = u;
    u = v - q * u;
    v = t;
  } while (u != 0);
  a = a % y;
  if (a < 0)
    a = y + a;
  return a;
}

/* return (a^b) mod m */
static int pow_mod(int a, int b, int m) 
{
  int r, aa;
  r = 1;
  aa = a;
  while (1) {
    if (b & 1)
      r = mul_mod(r, aa, m);
    b = b >> 1;
    if (b == 0)
      break;
    aa = mul_mod(aa, aa, m);
  }
  return r;
}

/* return true if n is prime */
static int is_prime(int n) 
{
  int r, i;
  if ((n % 2) == 0)
    return 0;
  r = (int)(sqrt(n));
  for (i = 3; i <= r; i += 2)
    if ((n % i) == 0)
      return 0;
  return 1;
}

/* return the prime number immediatly after n */
static int next_prime(int n) 
{
  do n++; while (!is_prime(n));
  return n;
}

int pi_nth_digit(int n) 
{
  int av, a, vmax, N, num, den, k, kq, kq2, t, v, s, i;
  double sum;
  N = (int)((n + 20) * log(10) / log(2));
  sum = 0;
  for (a = 3; a <= (2 * N); a = next_prime(a)) {
    vmax = (int)(log(2 * N) / log(a));
    av = 1;
    for (i = 0; i < vmax; i++)
      av = av * a;
    s = 0;
    num = 1;
    den = 1;
    v = 0;
    kq = 1;
    kq2 = 1;
    for (k = 1; k <= N; k++) {
      t = k;
      if (kq >= a) {
        do {
          t = t / a;
          v--;
        } while ((t % a) == 0);
        kq = 0;
      }
      kq++;
      num = mul_mod(num, t, av);
      t = (2 * k - 1);
      if (kq2 >= a) {
        if (kq2 == a) {
          do {
            t = t / a;
            v++;
          } while ((t % a) == 0);
        }
        kq2 -= a;
      }
      den = mul_mod(den, t, av);
      kq2 += 2;
      if (v > 0) {
        t = inv_mod(den, av);
        t = mul_mod(t, num, av);
        t = mul_mod(t, k, av);
        for (i = v; i < vmax; i++)
          t = mul_mod(t, a, av);
        s += t;
        if (s >= av)
          s -= av;
      }
    }
    t = pow_mod(10, n - 1, av);
    s = mul_mod(s, t, av);
    sum = fmod(sum + (double)s / (double)av, 1.0);
  }
  return (int)(sum * 1e9);
}

int main(int argc, char *argv[]) 
{
  int nmax, n;
  if (argc < 2 || (nmax = atoi(argv[1])) <= 0) {
    printf("This program computes n decimal digits of \\pi\n"
           "usage: %s n\n"
           "where n is the number of digits you want\n", argv[0]);
    return 1;
  }
  printf("3.");
  for (n = 1; n <= nmax; n += 9)
    printf("%09d", pi_nth_digit(n));
  printf("\n");
  return 0;
}
