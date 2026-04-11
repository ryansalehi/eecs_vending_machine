#ifndef QUESTION_H
#define QUESTION_H

#include <stdint.h>

#define NUM_COURSES (4)
#define QUESTION_LINES (3)
#define MAX_QUESTION_LINE (32)
#define MAX_ANSWER (36)
#define NUM_ANSWER (4)

// Data format for questions       -      formatting here for visual aid

//                                                     0      1      2      3
typedef enum Course                      { CINV=-1,  C373,  C270,  C281,  C489  } Course;

static const char* const course_str[NUM_COURSES] = { "373", "270", "281", "489" };
static const int         COURSE_INT[NUM_COURSES] = {  373,   270,   281,   489  };

static inline Course get_course(int course_int)
{
    for (int i = 0; i < 4; ++i) {
        if (COURSE_INT[i] == course_int)
        {
            return (Course)i;
        }
    }
    return CINV; // bad course
}

typedef struct {
    char question[QUESTION_LINES][MAX_QUESTION_LINE];
    char answers[NUM_ANSWER][MAX_ANSWER];
    uint8_t correct_answer; // could be char or index of correct answer
} Question;

// standin
static const Question questions[NUM_COURSES] = {
    [C373] = {
        .question = {
            "What ...?",
            "2nd line..."
        },
        .answers = {
            "A: ",
            "B: ",
            "C: ",
            "D: "
        },
        .correct_answer = 1 // index or letter is fine
    },
    { .question = {{0}}, .answers = {{0}}, .correct_answer = 0 },
    { .question = {{0}}, .answers = {{0}}, .correct_answer = 0 },
    { .question = {{0}}, .answers = {{0}}, .correct_answer = 0 }
};

#endif /* QUESTION_H */