# ForceSampleDelay

This project tries to manually and forcefully introduce a plugin delay to the effect chain in DAW through adjustable buffer for lookahead. But there is no changes to the signal. Used to match delays between different tracks without delay compensation.

Drag the lookahead knob to preview a delay from 0 to 5000 ms. The delay and reported plugin latency update when you release the mouse. You can also enter a value directly in the text box.

The **Bypass** button passes audio through without delay and reports zero latency to the host. Turning bypass off restores the selected lookahead. Bypass is available to the host as a parameter and is saved with the plugin state.

To build and run the optional regression tests, configure with `-DFORCE_SAMPLE_DELAY_BUILD_TESTS=ON`, build the `ForceSampleDelayTests` target, and run `ctest --test-dir <build-directory> -C Release --output-on-failure`.
