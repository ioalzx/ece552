#include "predictor.h"
#include "assert.h"
#include "math.h"

#define STRONGLY_NOT_TAKEN 0
#define WEAKLY_NOT_TAKEN 1
#define WEAKLY_TAKEN 2
#define STRONGLY_TAKEN 3


/////////////////////////////////////////////////////////////
// 2bitsat
/////////////////////////////////////////////////////////////

static int predictionTable [4096];

UINT32 getPredictionIndex(UINT32 PC) {
    return PC & 0xfff;   
}

// Lab 2 - Custom Code
// get prediction from a 2bit saturating counter which is a 1d array with a value of 0-3 defined at the top
bool makePrediction(UINT32 index, int* table) {
    switch(table[index]){
        case STRONGLY_NOT_TAKEN:
        case WEAKLY_NOT_TAKEN:
            return NOT_TAKEN;
        case WEAKLY_TAKEN:
        case STRONGLY_TAKEN:
            return TAKEN;
        default:
            //This is an unexpected case
            assert(false && "We should not be entering this case");  //Adding a default case to remove warning
            return 0;
    };
}

//update a 2 bit prediction counter which is a 1d array with a value of 0-3 defined at the top
void updatePrediction(bool resolveDir, UINT32 index, int* table) {
    
    if(NOT_TAKEN == resolveDir &&
       STRONGLY_NOT_TAKEN != table[index]){
        table[index]--;
    }
    else if(TAKEN == resolveDir &&
       STRONGLY_TAKEN != table[index]){
        table[index]++;
    }   
}

void InitPredictor_2bitsat() {
    int i;
    //initialize entire prediction table to weakly not taken
    for(i = 0; i < 4096; i++){
        predictionTable[i] = WEAKLY_NOT_TAKEN;
    }
}

bool GetPrediction_2bitsat(UINT32 PC) {

    UINT32 index = getPredictionIndex(PC);

    return makePrediction(index, predictionTable);

}

void UpdatePredictor_2bitsat(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {

    UINT32 index = getPredictionIndex(PC);   

    updatePrediction(resolveDir, index, predictionTable);

}

/////////////////////////////////////////////////////////////
// 2level
/////////////////////////////////////////////////////////////

static int bht[512];
static int pht[8][64];

UINT32 getBHTIndex(UINT32 PC) {
    return PC & 0xff8 >> 3;   
}

UINT32 getPHTIndex(UINT32 PC) {
    return PC & 0x7;   
}

int get_6bit_history(UINT32 index) {
    return bht[index] & 0x3f;
}

//get a prediction from the 2 level predictor
int make_two_level_prediction(UINT32 PC) {

    //get table addresses
    UINT32 bht_index = getBHTIndex(PC);

    UINT32 pht_row = getPHTIndex(PC);
    UINT32 pht_column = get_6bit_history(bht_index);

    return makePrediction(pht_column, pht[pht_row]);

}

// update a Branch history table
void record_history(UINT32 index, bool resolveDir, int* table) {
    table[index] = table[index] << 1 | resolveDir;
}

// update the 2 level predictor
void update_two_level_prediction(UINT32 PC, bool resolveDir) {

    // get table addresses
    UINT32 bht_index = getBHTIndex(PC);

    UINT32 pht_row = getPHTIndex(PC);
    UINT32 pht_column = get_6bit_history(bht_index);

    // update the pht
    updatePrediction(resolveDir, pht_column, pht[pht_row]);

    //update the bht
    record_history(bht_index, resolveDir, bht);
}

void InitPredictor_2level() {
    int i;

    int* tmp_pht = (int*)pht; // temperary pointer to the pht block of memory

    for (i = 0; i < 512; i++){
        bht[i] = 0;
        *tmp_pht++ = WEAKLY_NOT_TAKEN;
    }
}

bool GetPrediction_2level(UINT32 PC) {
    return make_two_level_prediction(PC);
}

void UpdatePredictor_2level(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    update_two_level_prediction(PC, resolveDir);
}

/////////////////////////////////////////////////////////////
// openend
/////////////////////////////////////////////////////////////

/**********************************************************
 * Perceptron learning predictor
 * ********************************************************/
const UINT32 PERCEPTRON_ROW = 512;
const long long PERCEPTRON_COL = 31;

const int  P_THRESHOLD = 71;

static char percep_table[PERCEPTRON_ROW][PERCEPTRON_COL];
static bool p_hist[PERCEPTRON_COL];

static int output = 0;

//Generic funtion to get index for our BHT and PHT tables
UINT32 maskAndShift(UINT32 src, UINT32 bitmask, UINT32 rightOffset) {
    return src & bitmask >> rightOffset;   
}

/******************************************** 
 * OUR PREDICTOR FUNCTIONS 
 * *****************************************/
/****** Perceptron Predictor Functions ******/

int GetPerceptronPrediction(UINT32 PC) {
    int row_index = PC % PERCEPTRON_ROW;
    int j, weight_sum = 0;
    for(j = 1; j < PERCEPTRON_COL; j++){
        int masked_weight = percep_table[row_index][j];     //8 bits for all weights
        if(p_hist[j]){
            //taken in global history
            weight_sum += masked_weight;
        }
        else{
            weight_sum += -1*masked_weight;
        }
    }
    int masked_bias = percep_table[row_index][0];
    output = masked_bias + weight_sum;
    return (output >= 0) ? TAKEN : NOT_TAKEN;
}

void UpdatePerceptronPredictor(UINT32 PC, bool resolveDir, bool predDir) {
    int i = PC % PERCEPTRON_ROW;
    if(resolveDir != predDir || abs(output) <= P_THRESHOLD){
        if(resolveDir == TAKEN){
            percep_table[i][0] += 1; 
        }
        else{
            percep_table[i][0] += -1; 
        }
        int j = 1;
        for(; j < PERCEPTRON_COL; j++){
            if(resolveDir == p_hist[j]){
                percep_table[i][j] += 1;
            }
            else{
                percep_table[i][j] += -1;
            }
        }
    }

    int k;

    for (k = 1; k < PERCEPTRON_COL-1; k++){
        p_hist[k] = p_hist[k+1];
    }
    p_hist[k] = (resolveDir == TAKEN);
}

void InitPredictor_openend() {
    //No initialization needed
}

bool GetPrediction_openend(UINT32 PC) { 
  return GetPerceptronPrediction(PC);
}

void UpdatePredictor_openend(UINT32 PC, bool resolveDir, bool predDir, UINT32 branchTarget) {
    UpdatePerceptronPredictor(PC, resolveDir, predDir);
}

