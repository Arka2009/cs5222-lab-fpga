# Sources
tcl/ - Build Scripts
bitstreams/
    final_classifier_fixed_baseline.bit - Part II fixed point Implementation
    final_classifier_fixed_optI.bit     - Part III fixed point Implementation with scaled
    									  down images and Linear Ridge Classifier with
    									  (alpha - 140.0)
    final_classifier_fixed_optII.bit    - Part III fixed point Implementation with scaled
                                          down images and Linear Ridge Classifier with 
                                          (alpha - 140.0) + Input matrix, Output Matrix
                                          and Weight matrix pipelined streaming
										  accesses.

jupyter/
    classifier_2.ipynb                  - PYNQ board script

hls/mmult_opt
										- Source HLS for final_classifier_fixed_optII.bit

hls/mmult_fixed
                                        - Source final_classifier_fixed_baseline.bit.
                                          final_classifier_fixed_optI.bit can be obtained
                                          by replacing FEAT to 144 in mmult.h and
                                          array partition block factor tp 72.

python/mnist.py                         - Classifier. Same of original code. Add the
                                          alpha value of 140 as the Linear ridge classifier
                                          parameter and run with --dim 12 to obtain the weights
                                          and offsets of *_optI and *_optII implementations
