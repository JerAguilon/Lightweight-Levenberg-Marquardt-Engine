#ifndef MY_GTSAM_SOLVER_H
#define MY_GTSAM_SOLVER_H
#define TOL 1e-30

#include <iostream>
#include <algorithm>
#include <math.h>

typedef double (*EvaluationFunction)(double *params, double *x);
typedef void (*GradientFunction)(double *gradient, double *params, double *x);

/**
 *  Solves the equation X[RowsMeasurements x RowsParam] * P[RowsParam] = Y[RowsMeasurements]
 *  todo(Jeremy): Add generic support for ColsY
 */
template<int RowsParams, int RowsMeasurements, int ColsY = 1 /* Stubbed since it is unsupported for now */>
class MyGTSAMSolver {
public:

    EvaluationFunction evaluationFunction;
    GradientFunction gradientFunction;

    double (&parameters)[RowsParams];
    double (&x)[RowsMeasurements][RowsParams];
    double (&y)[RowsMeasurements];

    MyGTSAMSolver(
        EvaluationFunction evaluationFunction,
        GradientFunction gradientFunction,
        double (&initialParams)[RowsParams],
        double (&x)[RowsMeasurements][RowsParams],
        double (&y)[RowsMeasurements]
    );

    bool fit();

private:
    double hessian[RowsParams][RowsParams],
           choleskyDecomposition[RowsParams][RowsParams];

    double derivative[RowsParams],
           gradient[RowsParams];

    double newParameters[RowsParams],
           delta[RowsParams];

    double getError(
        double (&parameters)[RowsParams],
        double (&x)[RowsMeasurements][RowsParams],
        double (&y)[RowsMeasurements]);

    bool getCholeskyDecomposition();
    void solveCholesky();
};

template<int RowsParameters, int RowsMeasurements, int ColsY>
MyGTSAMSolver<RowsParameters, RowsMeasurements, ColsY>::MyGTSAMSolver(
    EvaluationFunction evaluationFunction,
    GradientFunction gradientFunction,
    double (&initialParams)[RowsParameters],
    double (&x)[RowsMeasurements][RowsParameters],
    double (&y)[RowsMeasurements]
):
    evaluationFunction(evaluationFunction),
    gradientFunction(gradientFunction),
    parameters(initialParams),
    x(x),
    y(y),
    hessian{},
    choleskyDecomposition{},
    derivative{},
    gradient{},
    delta{},
    newParameters{}
{}

template<int RowsParameters, int RowsMeasurements, int ColsY>
double MyGTSAMSolver<RowsParameters, RowsMeasurements, ColsY>::getError(
    double (&parameters)[RowsParameters],
    double (&x)[RowsMeasurements][RowsParameters],
    double (&y)[RowsMeasurements])
{
    double residual;
    double error = 0;

    for (int i = 0; i < RowsMeasurements; i++) {
        residual = evaluationFunction(parameters, x[i]) - y[i];
        error += residual * residual;
    }
    return error;
}

template<int RowsParameters, int RowsMeasurements, int ColsY>
bool MyGTSAMSolver<RowsParameters, RowsMeasurements, ColsY>::fit() {
    // TODO: make these input arguments
    int maxIterations = 10000;
    double lambda = 0.1;
    double upFactor = 10.0;
    double downFactor = 1.0/10.0;
    double targetDeltaError = 0.01;

    double currentError = getError(parameters, x, y);

    int iteration;
    for (iteration=0; iteration < maxIterations; iteration++) {
        std::cout << "Current Error: " << currentError << std::endl;
        std::cout << "Mean Error: " << currentError / RowsMeasurements << std::endl << std::endl;


        for (int i = 0; i < RowsParameters; i++) {
            derivative[i] = 0.0;
        }

        for (int i = 0; i < RowsParameters; i++) {
            for (int j = 0; j < RowsParameters; j++) {
                hessian[i][j] = 0.0;
            }
        }

        // Build out the jacobian and the hessian matrices
        for (int m = 0; m < RowsMeasurements; m++) {
            double *currX = x[m];
            double currY = y[m];
            gradientFunction(gradient, parameters, currX);

            for (int i = 0; i < RowsParameters; i++) {
                // J_i = residual * gradient
                derivative[i] += (currY - evaluationFunction(parameters, currX)) * gradient[i];
                // H = J^T * J
                for (int j = 0; j <=i; j++) {
                    hessian[i][j] += gradient[i]*gradient[j];
                }
            }
        }

        double multFactor = 1 + lambda;
        bool illConditioned = true;
        double newError = 0;
        double deltaError = 0;

        while (illConditioned && iteration < maxIterations) {
            illConditioned = getCholeskyDecomposition();
            if (!illConditioned) {
                solveCholesky();
                for (int i = 0; i < RowsParameters; i++) {
                    newParameters[i] = parameters[i] + delta[i];
                }
                newError = getError(newParameters, x, y);
                deltaError = newError - currentError;
                illConditioned = deltaError > 0;
            }

            if (illConditioned) {
                multFactor = (1 + lambda * upFactor) / (1 + lambda);
                lambda *= upFactor;
                iteration++;
            }
        }

        for (int i = 0; i < RowsParameters; i++) {
            parameters[i] = newParameters[i];
        }

        currentError = newError;
        lambda *= downFactor;

        if (!illConditioned && (-deltaError < targetDeltaError)) break;
    }
    std::cout << "Current Error: " << currentError << std::endl;
    std::cout << "Mean Error: " << currentError / RowsMeasurements << std::endl << std::endl;
    return true;
}

template<int RowsParameters, int RowsMeasurements, int ColsY>
bool MyGTSAMSolver<RowsParameters, RowsMeasurements, ColsY>::getCholeskyDecomposition() {
    int i, j, k;
    double sum;

    int n = RowsParameters;

    for (i = 0; i < n; i++) {
        for (j = 0; j < i; j++) {
            sum = 0;
            for (k = 0; k < j; k++) {
                sum += choleskyDecomposition[i][k] * choleskyDecomposition[j][k];
            }
            choleskyDecomposition[i][j] = (hessian[i][j] - sum) / choleskyDecomposition[j][j];
        }

        sum = 0;
        for (k = 0; k < i; k++) {
            sum += choleskyDecomposition[i][k] * choleskyDecomposition[i][k];
        }
        sum = hessian[i][i] - sum;
        if (sum < TOL) return true;
        choleskyDecomposition[i][i] = sqrt(sum);
    }
    return 0;
}

template<int RowsParameters, int RowsMeasurements, int ColsY>
void MyGTSAMSolver<RowsParameters, RowsMeasurements, ColsY>::solveCholesky() {
    int i, j;
    double sum;

    int n = RowsParameters;

    for (i = 0; i < n; i++) {
        sum = 0;
        for (j = 0; j < i; j++) {
            sum += choleskyDecomposition[i][j] * delta[j];
        }
        delta[j] = (derivative[i] - sum) / choleskyDecomposition[i][i];
    }

    for (i = n - 1; i >= 0; i--) {
        sum = 0;
        for (j = i + 1; j < n; j++) {
            sum += choleskyDecomposition[j][i] * delta[j];
        }
        delta[i] = (delta[i] - sum) / choleskyDecomposition[i][i];
    }
}

#endif