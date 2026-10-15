#ifndef MNIST_H
#define MNIST_H

#include <Eigen/Dense>
#include <fstream>

struct Sample {
    Eigen::VectorXd x;
    Eigen::VectorXd y;
};

uint32_t read_uint32(std::ifstream& file);

Eigen::VectorXd load_image(std::ifstream& file, int index);

int load_label(std::ifstream& file, int index);

Sample load_sample(
    std::ifstream& image_file,
    std::ifstream& label_file,
    int index
);

#endif