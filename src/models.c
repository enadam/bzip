/*
 * models.c -- the structured model proper
 */

/* Include files */
#include "config.h"

#include <sys/types.h>

#include "main.h"
#include "models.h"

/*------------------------------------------------------*/
/* The parameters in these models and bogusModel	*/
/* -- specifically, the value of 1000 for max-total-	*/
/* frequency -- determine the lowest acceptable values	*/
/* for smallF and indirectly smallB in the arithmetic	*/
/* coder.						*/
/*------------------------------------------------------*/

/* Function prototypes */
static void initModel(struct Model *m);

/* Global variable definitions */
struct Model model_bogus = { 256,	0,	0 };
struct Model models[] =
{
	/* MODEL_BASIS   */ { 11,	12,	1000 },
	/* MODEL_2_3     */ { 2,	4,	1000 },
	/* MODEL_4_7     */ { 4,	3,	1000 },
	/* MODEL_8_15    */ { 8,	3,	1000 },
	/* MODEL_16_31   */ { 16,	3,	1000 },
	/* MODEL_32_63   */ { 32,	3,	1000 },
	/* MODEL_64_127  */ { 64,	2,	1000 },
	/* MODEL_128_255 */ { 128,	1,	1000 }
};

#ifdef CONFIG_COMPRESS
struct MTFVals_encode_st const MTFVals_encode[] =
{
	/*   0 */ { 0, NULL, 0 },
	/*   1 */ { 0, NULL, 0 },
	/*   2 */ { VAL_2_3, &models[MODEL_2_3], 2 & 1 },
	/*   3 */ { VAL_2_3, &models[MODEL_2_3], 3 & 1 },
	/*   4 */ { VAL_4_7, &models[MODEL_4_7], 4 & 3 },
	/*   5 */ { VAL_4_7, &models[MODEL_4_7], 5 & 3 },
	/*   6 */ { VAL_4_7, &models[MODEL_4_7], 6 & 3 },
	/*   7 */ { VAL_4_7, &models[MODEL_4_7], 7 & 3 },
	/*   8 */ { VAL_8_15, &models[MODEL_8_15], 8 & 7 },
	/*   9 */ { VAL_8_15, &models[MODEL_8_15], 9 & 7 },
	/*  10 */ { VAL_8_15, &models[MODEL_8_15], 10 & 7 },
	/*  11 */ { VAL_8_15, &models[MODEL_8_15], 11 & 7 },
	/*  12 */ { VAL_8_15, &models[MODEL_8_15], 12 & 7 },
	/*  13 */ { VAL_8_15, &models[MODEL_8_15], 13 & 7 },
	/*  14 */ { VAL_8_15, &models[MODEL_8_15], 14 & 7 },
	/*  15 */ { VAL_8_15, &models[MODEL_8_15], 15 & 7 },
	/*  16 */ { VAL_16_31, &models[MODEL_16_31], 16 & 15 },
	/*  17 */ { VAL_16_31, &models[MODEL_16_31], 17 & 15 },
	/*  18 */ { VAL_16_31, &models[MODEL_16_31], 18 & 15 },
	/*  19 */ { VAL_16_31, &models[MODEL_16_31], 19 & 15 },
	/*  20 */ { VAL_16_31, &models[MODEL_16_31], 20 & 15 },
	/*  21 */ { VAL_16_31, &models[MODEL_16_31], 21 & 15 },
	/*  22 */ { VAL_16_31, &models[MODEL_16_31], 22 & 15 },
	/*  23 */ { VAL_16_31, &models[MODEL_16_31], 23 & 15 },
	/*  24 */ { VAL_16_31, &models[MODEL_16_31], 24 & 15 },
	/*  25 */ { VAL_16_31, &models[MODEL_16_31], 25 & 15 },
	/*  26 */ { VAL_16_31, &models[MODEL_16_31], 26 & 15 },
	/*  27 */ { VAL_16_31, &models[MODEL_16_31], 27 & 15 },
	/*  28 */ { VAL_16_31, &models[MODEL_16_31], 28 & 15 },
	/*  29 */ { VAL_16_31, &models[MODEL_16_31], 29 & 15 },
	/*  30 */ { VAL_16_31, &models[MODEL_16_31], 30 & 15 },
	/*  31 */ { VAL_16_31, &models[MODEL_16_31], 31 & 15 },
	/*  32 */ { VAL_32_63, &models[MODEL_32_63], 32 & 31 },
	/*  33 */ { VAL_32_63, &models[MODEL_32_63], 33 & 31 },
	/*  34 */ { VAL_32_63, &models[MODEL_32_63], 34 & 31 },
	/*  35 */ { VAL_32_63, &models[MODEL_32_63], 35 & 31 },
	/*  36 */ { VAL_32_63, &models[MODEL_32_63], 36 & 31 },
	/*  37 */ { VAL_32_63, &models[MODEL_32_63], 37 & 31 },
	/*  38 */ { VAL_32_63, &models[MODEL_32_63], 38 & 31 },
	/*  39 */ { VAL_32_63, &models[MODEL_32_63], 39 & 31 },
	/*  40 */ { VAL_32_63, &models[MODEL_32_63], 40 & 31 },
	/*  41 */ { VAL_32_63, &models[MODEL_32_63], 41 & 31 },
	/*  42 */ { VAL_32_63, &models[MODEL_32_63], 42 & 31 },
	/*  43 */ { VAL_32_63, &models[MODEL_32_63], 43 & 31 },
	/*  44 */ { VAL_32_63, &models[MODEL_32_63], 44 & 31 },
	/*  45 */ { VAL_32_63, &models[MODEL_32_63], 45 & 31 },
	/*  46 */ { VAL_32_63, &models[MODEL_32_63], 46 & 31 },
	/*  47 */ { VAL_32_63, &models[MODEL_32_63], 47 & 31 },
	/*  48 */ { VAL_32_63, &models[MODEL_32_63], 48 & 31 },
	/*  49 */ { VAL_32_63, &models[MODEL_32_63], 49 & 31 },
	/*  50 */ { VAL_32_63, &models[MODEL_32_63], 50 & 31 },
	/*  51 */ { VAL_32_63, &models[MODEL_32_63], 51 & 31 },
	/*  52 */ { VAL_32_63, &models[MODEL_32_63], 52 & 31 },
	/*  53 */ { VAL_32_63, &models[MODEL_32_63], 53 & 31 },
	/*  54 */ { VAL_32_63, &models[MODEL_32_63], 54 & 31 },
	/*  55 */ { VAL_32_63, &models[MODEL_32_63], 55 & 31 },
	/*  56 */ { VAL_32_63, &models[MODEL_32_63], 56 & 31 },
	/*  57 */ { VAL_32_63, &models[MODEL_32_63], 57 & 31 },
	/*  58 */ { VAL_32_63, &models[MODEL_32_63], 58 & 31 },
	/*  59 */ { VAL_32_63, &models[MODEL_32_63], 59 & 31 },
	/*  60 */ { VAL_32_63, &models[MODEL_32_63], 60 & 31 },
	/*  61 */ { VAL_32_63, &models[MODEL_32_63], 61 & 31 },
	/*  62 */ { VAL_32_63, &models[MODEL_32_63], 62 & 31 },
	/*  63 */ { VAL_32_63, &models[MODEL_32_63], 63 & 31 },
	/*  64 */ { VAL_64_127, &models[MODEL_64_127], 64 & 63 },
	/*  65 */ { VAL_64_127, &models[MODEL_64_127], 65 & 63 },
	/*  66 */ { VAL_64_127, &models[MODEL_64_127], 66 & 63 },
	/*  67 */ { VAL_64_127, &models[MODEL_64_127], 67 & 63 },
	/*  68 */ { VAL_64_127, &models[MODEL_64_127], 68 & 63 },
	/*  69 */ { VAL_64_127, &models[MODEL_64_127], 69 & 63 },
	/*  70 */ { VAL_64_127, &models[MODEL_64_127], 70 & 63 },
	/*  71 */ { VAL_64_127, &models[MODEL_64_127], 71 & 63 },
	/*  72 */ { VAL_64_127, &models[MODEL_64_127], 72 & 63 },
	/*  73 */ { VAL_64_127, &models[MODEL_64_127], 73 & 63 },
	/*  74 */ { VAL_64_127, &models[MODEL_64_127], 74 & 63 },
	/*  75 */ { VAL_64_127, &models[MODEL_64_127], 75 & 63 },
	/*  76 */ { VAL_64_127, &models[MODEL_64_127], 76 & 63 },
	/*  77 */ { VAL_64_127, &models[MODEL_64_127], 77 & 63 },
	/*  78 */ { VAL_64_127, &models[MODEL_64_127], 78 & 63 },
	/*  79 */ { VAL_64_127, &models[MODEL_64_127], 79 & 63 },
	/*  80 */ { VAL_64_127, &models[MODEL_64_127], 80 & 63 },
	/*  81 */ { VAL_64_127, &models[MODEL_64_127], 81 & 63 },
	/*  82 */ { VAL_64_127, &models[MODEL_64_127], 82 & 63 },
	/*  83 */ { VAL_64_127, &models[MODEL_64_127], 83 & 63 },
	/*  84 */ { VAL_64_127, &models[MODEL_64_127], 84 & 63 },
	/*  85 */ { VAL_64_127, &models[MODEL_64_127], 85 & 63 },
	/*  86 */ { VAL_64_127, &models[MODEL_64_127], 86 & 63 },
	/*  87 */ { VAL_64_127, &models[MODEL_64_127], 87 & 63 },
	/*  88 */ { VAL_64_127, &models[MODEL_64_127], 88 & 63 },
	/*  89 */ { VAL_64_127, &models[MODEL_64_127], 89 & 63 },
	/*  90 */ { VAL_64_127, &models[MODEL_64_127], 90 & 63 },
	/*  91 */ { VAL_64_127, &models[MODEL_64_127], 91 & 63 },
	/*  92 */ { VAL_64_127, &models[MODEL_64_127], 92 & 63 },
	/*  93 */ { VAL_64_127, &models[MODEL_64_127], 93 & 63 },
	/*  94 */ { VAL_64_127, &models[MODEL_64_127], 94 & 63 },
	/*  95 */ { VAL_64_127, &models[MODEL_64_127], 95 & 63 },
	/*  96 */ { VAL_64_127, &models[MODEL_64_127], 96 & 63 },
	/*  97 */ { VAL_64_127, &models[MODEL_64_127], 97 & 63 },
	/*  98 */ { VAL_64_127, &models[MODEL_64_127], 98 & 63 },
	/*  99 */ { VAL_64_127, &models[MODEL_64_127], 99 & 63 },
	/* 100 */ { VAL_64_127, &models[MODEL_64_127], 100 & 63 },
	/* 101 */ { VAL_64_127, &models[MODEL_64_127], 101 & 63 },
	/* 102 */ { VAL_64_127, &models[MODEL_64_127], 102 & 63 },
	/* 103 */ { VAL_64_127, &models[MODEL_64_127], 103 & 63 },
	/* 104 */ { VAL_64_127, &models[MODEL_64_127], 104 & 63 },
	/* 105 */ { VAL_64_127, &models[MODEL_64_127], 105 & 63 },
	/* 106 */ { VAL_64_127, &models[MODEL_64_127], 106 & 63 },
	/* 107 */ { VAL_64_127, &models[MODEL_64_127], 107 & 63 },
	/* 108 */ { VAL_64_127, &models[MODEL_64_127], 108 & 63 },
	/* 109 */ { VAL_64_127, &models[MODEL_64_127], 109 & 63 },
	/* 110 */ { VAL_64_127, &models[MODEL_64_127], 110 & 63 },
	/* 111 */ { VAL_64_127, &models[MODEL_64_127], 111 & 63 },
	/* 112 */ { VAL_64_127, &models[MODEL_64_127], 112 & 63 },
	/* 113 */ { VAL_64_127, &models[MODEL_64_127], 113 & 63 },
	/* 114 */ { VAL_64_127, &models[MODEL_64_127], 114 & 63 },
	/* 115 */ { VAL_64_127, &models[MODEL_64_127], 115 & 63 },
	/* 116 */ { VAL_64_127, &models[MODEL_64_127], 116 & 63 },
	/* 117 */ { VAL_64_127, &models[MODEL_64_127], 117 & 63 },
	/* 118 */ { VAL_64_127, &models[MODEL_64_127], 118 & 63 },
	/* 119 */ { VAL_64_127, &models[MODEL_64_127], 119 & 63 },
	/* 120 */ { VAL_64_127, &models[MODEL_64_127], 120 & 63 },
	/* 121 */ { VAL_64_127, &models[MODEL_64_127], 121 & 63 },
	/* 122 */ { VAL_64_127, &models[MODEL_64_127], 122 & 63 },
	/* 123 */ { VAL_64_127, &models[MODEL_64_127], 123 & 63 },
	/* 124 */ { VAL_64_127, &models[MODEL_64_127], 124 & 63 },
	/* 125 */ { VAL_64_127, &models[MODEL_64_127], 125 & 63 },
	/* 126 */ { VAL_64_127, &models[MODEL_64_127], 126 & 63 },
	/* 127 */ { VAL_64_127, &models[MODEL_64_127], 127 & 63 },
	/* 128 */ { VAL_128_255, &models[MODEL_128_255], 128 & 127 },
	/* 129 */ { VAL_128_255, &models[MODEL_128_255], 129 & 127 },
	/* 130 */ { VAL_128_255, &models[MODEL_128_255], 130 & 127 },
	/* 131 */ { VAL_128_255, &models[MODEL_128_255], 131 & 127 },
	/* 132 */ { VAL_128_255, &models[MODEL_128_255], 132 & 127 },
	/* 133 */ { VAL_128_255, &models[MODEL_128_255], 133 & 127 },
	/* 134 */ { VAL_128_255, &models[MODEL_128_255], 134 & 127 },
	/* 135 */ { VAL_128_255, &models[MODEL_128_255], 135 & 127 },
	/* 136 */ { VAL_128_255, &models[MODEL_128_255], 136 & 127 },
	/* 137 */ { VAL_128_255, &models[MODEL_128_255], 137 & 127 },
	/* 138 */ { VAL_128_255, &models[MODEL_128_255], 138 & 127 },
	/* 139 */ { VAL_128_255, &models[MODEL_128_255], 139 & 127 },
	/* 140 */ { VAL_128_255, &models[MODEL_128_255], 140 & 127 },
	/* 141 */ { VAL_128_255, &models[MODEL_128_255], 141 & 127 },
	/* 142 */ { VAL_128_255, &models[MODEL_128_255], 142 & 127 },
	/* 143 */ { VAL_128_255, &models[MODEL_128_255], 143 & 127 },
	/* 144 */ { VAL_128_255, &models[MODEL_128_255], 144 & 127 },
	/* 145 */ { VAL_128_255, &models[MODEL_128_255], 145 & 127 },
	/* 146 */ { VAL_128_255, &models[MODEL_128_255], 146 & 127 },
	/* 147 */ { VAL_128_255, &models[MODEL_128_255], 147 & 127 },
	/* 148 */ { VAL_128_255, &models[MODEL_128_255], 148 & 127 },
	/* 149 */ { VAL_128_255, &models[MODEL_128_255], 149 & 127 },
	/* 150 */ { VAL_128_255, &models[MODEL_128_255], 150 & 127 },
	/* 151 */ { VAL_128_255, &models[MODEL_128_255], 151 & 127 },
	/* 152 */ { VAL_128_255, &models[MODEL_128_255], 152 & 127 },
	/* 153 */ { VAL_128_255, &models[MODEL_128_255], 153 & 127 },
	/* 154 */ { VAL_128_255, &models[MODEL_128_255], 154 & 127 },
	/* 155 */ { VAL_128_255, &models[MODEL_128_255], 155 & 127 },
	/* 156 */ { VAL_128_255, &models[MODEL_128_255], 156 & 127 },
	/* 157 */ { VAL_128_255, &models[MODEL_128_255], 157 & 127 },
	/* 158 */ { VAL_128_255, &models[MODEL_128_255], 158 & 127 },
	/* 159 */ { VAL_128_255, &models[MODEL_128_255], 159 & 127 },
	/* 160 */ { VAL_128_255, &models[MODEL_128_255], 160 & 127 },
	/* 161 */ { VAL_128_255, &models[MODEL_128_255], 161 & 127 },
	/* 162 */ { VAL_128_255, &models[MODEL_128_255], 162 & 127 },
	/* 163 */ { VAL_128_255, &models[MODEL_128_255], 163 & 127 },
	/* 164 */ { VAL_128_255, &models[MODEL_128_255], 164 & 127 },
	/* 165 */ { VAL_128_255, &models[MODEL_128_255], 165 & 127 },
	/* 166 */ { VAL_128_255, &models[MODEL_128_255], 166 & 127 },
	/* 167 */ { VAL_128_255, &models[MODEL_128_255], 167 & 127 },
	/* 168 */ { VAL_128_255, &models[MODEL_128_255], 168 & 127 },
	/* 169 */ { VAL_128_255, &models[MODEL_128_255], 169 & 127 },
	/* 170 */ { VAL_128_255, &models[MODEL_128_255], 170 & 127 },
	/* 171 */ { VAL_128_255, &models[MODEL_128_255], 171 & 127 },
	/* 172 */ { VAL_128_255, &models[MODEL_128_255], 172 & 127 },
	/* 173 */ { VAL_128_255, &models[MODEL_128_255], 173 & 127 },
	/* 174 */ { VAL_128_255, &models[MODEL_128_255], 174 & 127 },
	/* 175 */ { VAL_128_255, &models[MODEL_128_255], 175 & 127 },
	/* 176 */ { VAL_128_255, &models[MODEL_128_255], 176 & 127 },
	/* 177 */ { VAL_128_255, &models[MODEL_128_255], 177 & 127 },
	/* 178 */ { VAL_128_255, &models[MODEL_128_255], 178 & 127 },
	/* 179 */ { VAL_128_255, &models[MODEL_128_255], 179 & 127 },
	/* 180 */ { VAL_128_255, &models[MODEL_128_255], 180 & 127 },
	/* 181 */ { VAL_128_255, &models[MODEL_128_255], 181 & 127 },
	/* 182 */ { VAL_128_255, &models[MODEL_128_255], 182 & 127 },
	/* 183 */ { VAL_128_255, &models[MODEL_128_255], 183 & 127 },
	/* 184 */ { VAL_128_255, &models[MODEL_128_255], 184 & 127 },
	/* 185 */ { VAL_128_255, &models[MODEL_128_255], 185 & 127 },
	/* 186 */ { VAL_128_255, &models[MODEL_128_255], 186 & 127 },
	/* 187 */ { VAL_128_255, &models[MODEL_128_255], 187 & 127 },
	/* 188 */ { VAL_128_255, &models[MODEL_128_255], 188 & 127 },
	/* 189 */ { VAL_128_255, &models[MODEL_128_255], 189 & 127 },
	/* 190 */ { VAL_128_255, &models[MODEL_128_255], 190 & 127 },
	/* 191 */ { VAL_128_255, &models[MODEL_128_255], 191 & 127 },
	/* 192 */ { VAL_128_255, &models[MODEL_128_255], 192 & 127 },
	/* 193 */ { VAL_128_255, &models[MODEL_128_255], 193 & 127 },
	/* 194 */ { VAL_128_255, &models[MODEL_128_255], 194 & 127 },
	/* 195 */ { VAL_128_255, &models[MODEL_128_255], 195 & 127 },
	/* 196 */ { VAL_128_255, &models[MODEL_128_255], 196 & 127 },
	/* 197 */ { VAL_128_255, &models[MODEL_128_255], 197 & 127 },
	/* 198 */ { VAL_128_255, &models[MODEL_128_255], 198 & 127 },
	/* 199 */ { VAL_128_255, &models[MODEL_128_255], 199 & 127 },
	/* 200 */ { VAL_128_255, &models[MODEL_128_255], 200 & 127 },
	/* 201 */ { VAL_128_255, &models[MODEL_128_255], 201 & 127 },
	/* 202 */ { VAL_128_255, &models[MODEL_128_255], 202 & 127 },
	/* 203 */ { VAL_128_255, &models[MODEL_128_255], 203 & 127 },
	/* 204 */ { VAL_128_255, &models[MODEL_128_255], 204 & 127 },
	/* 205 */ { VAL_128_255, &models[MODEL_128_255], 205 & 127 },
	/* 206 */ { VAL_128_255, &models[MODEL_128_255], 206 & 127 },
	/* 207 */ { VAL_128_255, &models[MODEL_128_255], 207 & 127 },
	/* 208 */ { VAL_128_255, &models[MODEL_128_255], 208 & 127 },
	/* 209 */ { VAL_128_255, &models[MODEL_128_255], 209 & 127 },
	/* 210 */ { VAL_128_255, &models[MODEL_128_255], 210 & 127 },
	/* 211 */ { VAL_128_255, &models[MODEL_128_255], 211 & 127 },
	/* 212 */ { VAL_128_255, &models[MODEL_128_255], 212 & 127 },
	/* 213 */ { VAL_128_255, &models[MODEL_128_255], 213 & 127 },
	/* 214 */ { VAL_128_255, &models[MODEL_128_255], 214 & 127 },
	/* 215 */ { VAL_128_255, &models[MODEL_128_255], 215 & 127 },
	/* 216 */ { VAL_128_255, &models[MODEL_128_255], 216 & 127 },
	/* 217 */ { VAL_128_255, &models[MODEL_128_255], 217 & 127 },
	/* 218 */ { VAL_128_255, &models[MODEL_128_255], 218 & 127 },
	/* 219 */ { VAL_128_255, &models[MODEL_128_255], 219 & 127 },
	/* 220 */ { VAL_128_255, &models[MODEL_128_255], 220 & 127 },
	/* 221 */ { VAL_128_255, &models[MODEL_128_255], 221 & 127 },
	/* 222 */ { VAL_128_255, &models[MODEL_128_255], 222 & 127 },
	/* 223 */ { VAL_128_255, &models[MODEL_128_255], 223 & 127 },
	/* 224 */ { VAL_128_255, &models[MODEL_128_255], 224 & 127 },
	/* 225 */ { VAL_128_255, &models[MODEL_128_255], 225 & 127 },
	/* 226 */ { VAL_128_255, &models[MODEL_128_255], 226 & 127 },
	/* 227 */ { VAL_128_255, &models[MODEL_128_255], 227 & 127 },
	/* 228 */ { VAL_128_255, &models[MODEL_128_255], 228 & 127 },
	/* 229 */ { VAL_128_255, &models[MODEL_128_255], 229 & 127 },
	/* 230 */ { VAL_128_255, &models[MODEL_128_255], 230 & 127 },
	/* 231 */ { VAL_128_255, &models[MODEL_128_255], 231 & 127 },
	/* 232 */ { VAL_128_255, &models[MODEL_128_255], 232 & 127 },
	/* 233 */ { VAL_128_255, &models[MODEL_128_255], 233 & 127 },
	/* 234 */ { VAL_128_255, &models[MODEL_128_255], 234 & 127 },
	/* 235 */ { VAL_128_255, &models[MODEL_128_255], 235 & 127 },
	/* 236 */ { VAL_128_255, &models[MODEL_128_255], 236 & 127 },
	/* 237 */ { VAL_128_255, &models[MODEL_128_255], 237 & 127 },
	/* 238 */ { VAL_128_255, &models[MODEL_128_255], 238 & 127 },
	/* 239 */ { VAL_128_255, &models[MODEL_128_255], 239 & 127 },
	/* 240 */ { VAL_128_255, &models[MODEL_128_255], 240 & 127 },
	/* 241 */ { VAL_128_255, &models[MODEL_128_255], 241 & 127 },
	/* 242 */ { VAL_128_255, &models[MODEL_128_255], 242 & 127 },
	/* 243 */ { VAL_128_255, &models[MODEL_128_255], 243 & 127 },
	/* 244 */ { VAL_128_255, &models[MODEL_128_255], 244 & 127 },
	/* 245 */ { VAL_128_255, &models[MODEL_128_255], 245 & 127 },
	/* 246 */ { VAL_128_255, &models[MODEL_128_255], 246 & 127 },
	/* 247 */ { VAL_128_255, &models[MODEL_128_255], 247 & 127 },
	/* 248 */ { VAL_128_255, &models[MODEL_128_255], 248 & 127 },
	/* 249 */ { VAL_128_255, &models[MODEL_128_255], 249 & 127 },
	/* 250 */ { VAL_128_255, &models[MODEL_128_255], 250 & 127 },
	/* 251 */ { VAL_128_255, &models[MODEL_128_255], 251 & 127 },
	/* 252 */ { VAL_128_255, &models[MODEL_128_255], 252 & 127 },
	/* 253 */ { VAL_128_255, &models[MODEL_128_255], 253 & 127 },
	/* 254 */ { VAL_128_255, &models[MODEL_128_255], 254 & 127 },
	/* 255 */ { VAL_128_255, &models[MODEL_128_255], 255 & 127 }
};
#endif /* CONFIG_COMPRESS */

#ifdef CONFIG_DECOMPRESS
struct MTFVals_decode_st const MTFVals_decode[] =
{
	/* VAL_RUNA    */ { NULL, 0 },
	/* VAL_RUNB    */ { NULL, 0 },
	/* VAL_ONE     */ { NULL, 0 },
	/* VAL_2_3     */ { &models[MODEL_2_3], 2 },
	/* VAL_4_7     */ { &models[MODEL_4_7], 4 },
	/* VAL_8_15    */ { &models[MODEL_8_15], 8 },
	/* VAL_16_31   */ { &models[MODEL_16_31], 16 },
	/* VAL_32_63   */ { &models[MODEL_32_63], 32 },
	/* VAL_64_127  */ { &models[MODEL_64_127], 64 },
	/* VAL_128_255 */ { &models[MODEL_128_255], 128 }
};
#endif /* CONFIG_DECOMPRESS */

/* Program code */
/* Interface functions */
void initModels(void)
{
	unsigned i;

	for (i = 0; i < MODEL_LAST; i++)
		initModel(&models[i]);
} /* initModels */

void initBogusModel(void)
{
	unsigned i;

	model_bogus.totFreq = model_bogus.numSymbols;
	for (i = 0; i < model_bogus.numSymbols; i++)
		model_bogus.freq[i] = 1;
} /* initBogusModel */

/* Private functions */
void initModel(struct Model *m)
{
	unsigned i;

	m->totFreq = m->incValue * m->numSymbols;
	for (i = 0; i < m->numSymbols; i++)
		m->freq[i] = m->incValue;
} /* initModel */

/* End of models.c */
