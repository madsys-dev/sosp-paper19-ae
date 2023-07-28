enum {
    NUM_SOCKET = 2,
    NUM_PHYSICAL_CPU_PER_SOCKET = 26,
    SMT_LEVEL = 2,
};

const int OS_CPU_ID[NUM_SOCKET][NUM_PHYSICAL_CPU_PER_SOCKET][SMT_LEVEL] = {
    { /* socket id: 0 */
        { /* physical cpu id: 0 */
          0, 52,     },
        { /* physical cpu id: 1 */
          1, 53,     },
        { /* physical cpu id: 2 */
          2, 54,     },
        { /* physical cpu id: 3 */
          3, 55,     },
        { /* physical cpu id: 4 */
          4, 56,     },
        { /* physical cpu id: 5 */
          5, 57,     },
        { /* physical cpu id: 6 */
          6, 58,     },
        { /* physical cpu id: 8 */
          7, 59,     },
        { /* physical cpu id: 9 */
          8, 60,     },
        { /* physical cpu id: 10 */
          9, 61,     },
        { /* physical cpu id: 11 */
          10, 62,     },
        { /* physical cpu id: 12 */
          11, 63,     },
        { /* physical cpu id: 13 */
          12, 64,     },
        { /* physical cpu id: 16 */
          13, 65,     },
        { /* physical cpu id: 17 */
          14, 66,     },
        { /* physical cpu id: 18 */
          15, 67,     },
        { /* physical cpu id: 19 */
          16, 68,     },
        { /* physical cpu id: 20 */
          17, 69,     },
        { /* physical cpu id: 21 */
          18, 70,     },
        { /* physical cpu id: 22 */
          19, 71,     },
        { /* physical cpu id: 24 */
          20, 72,     },
        { /* physical cpu id: 25 */
          21, 73,     },
        { /* physical cpu id: 26 */
          22, 74,     },
        { /* physical cpu id: 27 */
          23, 75,     },
        { /* physical cpu id: 28 */
          24, 76,     },
        { /* physical cpu id: 29 */
          25, 77,     },
    },
    { /* socket id: 1 */
        { /* physical cpu id: 0 */
          26, 78,     },
        { /* physical cpu id: 1 */
          27, 79,     },
        { /* physical cpu id: 2 */
          28, 80,     },
        { /* physical cpu id: 3 */
          29, 81,     },
        { /* physical cpu id: 4 */
          30, 82,     },
        { /* physical cpu id: 5 */
          31, 83,     },
        { /* physical cpu id: 6 */
          32, 84,     },
        { /* physical cpu id: 8 */
          33, 85,     },
        { /* physical cpu id: 9 */
          34, 86,     },
        { /* physical cpu id: 10 */
          35, 87,     },
        { /* physical cpu id: 11 */
          36, 88,     },
        { /* physical cpu id: 12 */
          37, 89,     },
        { /* physical cpu id: 13 */
          38, 90,     },
        { /* physical cpu id: 16 */
          39, 91,     },
        { /* physical cpu id: 17 */
          40, 92,     },
        { /* physical cpu id: 18 */
          41, 93,     },
        { /* physical cpu id: 19 */
          42, 94,     },
        { /* physical cpu id: 20 */
          43, 95,     },
        { /* physical cpu id: 21 */
          44, 96,     },
        { /* physical cpu id: 22 */
          45, 97,     },
        { /* physical cpu id: 24 */
          46, 98,     },
        { /* physical cpu id: 25 */
          47, 99,     },
        { /* physical cpu id: 26 */
          48, 100,     },
        { /* physical cpu id: 27 */
          49, 101,     },
        { /* physical cpu id: 28 */
          50, 102,     },
        { /* physical cpu id: 29 */
          51, 103,     },
    },
};
const int OS_CPU_ID_2[NUM_SOCKET-1][NUM_PHYSICAL_CPU_PER_SOCKET][SMT_LEVEL] = {
    { /* socket id: 1 */
        { /* physical cpu id: 0 */
          26, 78,     },
        { /* physical cpu id: 1 */
          27, 79,     },
        { /* physical cpu id: 2 */
          28, 80,     },
        { /* physical cpu id: 3 */
          29, 81,     },
        { /* physical cpu id: 4 */
          30, 82,     },
        { /* physical cpu id: 5 */
          31, 83,     },
        { /* physical cpu id: 6 */
          32, 84,     },
        { /* physical cpu id: 8 */
          33, 85,     },
        { /* physical cpu id: 9 */
          34, 86,     },
        { /* physical cpu id: 10 */
          35, 87,     },
        { /* physical cpu id: 11 */
          36, 88,     },
        { /* physical cpu id: 12 */
          37, 89,     },
        { /* physical cpu id: 13 */
          38, 90,     },
        { /* physical cpu id: 16 */
          39, 91,     },
        { /* physical cpu id: 17 */
          40, 92,     },
        { /* physical cpu id: 18 */
          41, 93,     },
        { /* physical cpu id: 19 */
          42, 94,     },
        { /* physical cpu id: 20 */
          43, 95,     },
        { /* physical cpu id: 21 */
          44, 96,     },
        { /* physical cpu id: 22 */
          45, 97,     },
        { /* physical cpu id: 24 */
          46, 98,     },
        { /* physical cpu id: 25 */
          47, 99,     },
        { /* physical cpu id: 26 */
          48, 100,     },
        { /* physical cpu id: 27 */
          49, 101,     },
        { /* physical cpu id: 28 */
          50, 102,     },
        { /* physical cpu id: 29 */
          51, 103,     },
    },
};
