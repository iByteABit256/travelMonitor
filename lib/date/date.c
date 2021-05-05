#include <stdio.h>
#include <stdlib.h>
#include "date.h"

// d1 > d2  ->  1
// d1 == d2 ->  0
// d1 < d2  -> -1
int compareDates(Date d1, Date d2){
    if(d1->year > d2->year){
        return 1; 
    }else if(d1->year < d2->year){
        return -1;
    }

    if(d1->month > d2->month){
        return 1;
    }else if(d1->month < d2->month){
        return -1;
    }

    if(d1->day > d2->day){
        return 1;
    }else if(d1->day < d2->day){
        return -1;
    }

    return 0;
}
