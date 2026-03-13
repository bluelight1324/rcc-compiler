extern int printf(char *fmt, ...);

int day_type(int day) {
    int result;
    result = 0;
    switch (day) {
        case 1:
        case 2:
        case 3:
        case 4:
        case 5:
            result = 1;
            break;
        case 6:
        case 7:
            result = 2;
            break;
        default:
            result = 0;
            break;
    }
    return result;
}

int grade_letter(int score) {
    int grade;
    grade = 0;
    if (score >= 90) grade = 65;
    else if (score >= 80) grade = 66;
    else if (score >= 70) grade = 67;
    else if (score >= 60) grade = 68;
    else grade = 70;
    return grade;
}

int main() {
    int i;
    printf("=== Switch/Case Test ===\n");
    i = 1;
    while (i <= 8) {
        int t;
        t = day_type(i);
        if (t == 1) printf("Day %d: weekday\n", i);
        else if (t == 2) printf("Day %d: weekend\n", i);
        else printf("Day %d: invalid\n", i);
        i = i + 1;
    }

    printf("\n=== Grade Test ===\n");
    printf("Score 95 -> %c\n", grade_letter(95));
    printf("Score 85 -> %c\n", grade_letter(85));
    printf("Score 75 -> %c\n", grade_letter(75));
    printf("Score 65 -> %c\n", grade_letter(65));
    printf("Score 55 -> %c\n", grade_letter(55));
    return 0;
}
