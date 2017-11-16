#include <quad.h>

quad_t __divdi3(quad_t a,quad_t b) {
        u_quad_t ua = 0x0, ub = 0x0, uq = 0x0;
        int neg;

        if (a < 0)
                ua = -(u_quad_t)a, neg = 1;
        else
                ua = a, neg = 0;
        if (b < 0)
                ub = -(u_quad_t)b, neg ^= 1;
        else
                ub = b;
//        uq = __qdivrem(ua, ub, (u_quad_t *)0);
        return (neg ? -uq : uq);
}
