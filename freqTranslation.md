## Frequency Translation with Quadrature Oscillator

Reference: Lyons, R.G. (2011): Understanding Digital Processing. – Pearson, 3rd edition, Chapters 8, 13.32

### Quadrature Oscillator

From Lyons 13.32:

$$y_{i}(n) = y_{i}(n-1)\cos(\theta) - jy_{q}(n-1)\sin(\theta) \tag{13-134}$$

$$y_{q}(n) = y_{i}(n-1)\sin(\theta) + jy_{q}(n-1)\cos(\theta) \tag{13-134'}$$

### Digitalized Signal

$$x(n) = I(n) + jQ(n)$$

### Translated Signal

$$x'(n) = (I(n) + jQ(n)) \cdot (y_{i}(n) + jy_{q}(n))$$

$$x'(n) = (I(n)y_{i}(n) + jQ(n)y_{i}(n)) + (jI(n)y_{q}(n) + j^2Q(n)y_{q}(n))$$

Simplifying we get:

$$x'(n) = (I(n)y_{i}(n) - Q(n)y_{q}(n)) + j(I(n)y_{q}(n) + Q(n)y_{i}(n))$$

### Resolving Spectral Inversion from High-side Injection Use in T41

We need to consider that with high-side injection, the local oscillator sits *above* the RF signal causing spectral inversion in the QSD. The signal has been up-converted to baseband in *FreqShift1* per Lyons 13.1.2 (page 591), with a note *"with the savings of not having to shift/rotate the FFT_buffer"*.
