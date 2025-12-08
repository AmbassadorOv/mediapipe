# MediaPipe

![MediaPipe](https://developers.google.com/static/mediapipe/images/home/hero_01_1920.png)

**On-device machine learning for everyone**

MediaPipe is a framework for building cross-platform, customizable machine learning solutions for live and streaming media. It provides everything you need to customize and deploy to mobile (Android, iOS), web, desktop, edge devices, and IoT, effortlessly.

**Attention:** The primary developer documentation has moved to [https://developers.google.com/mediapipe](https://developers.google.com/mediapipe). This README provides a guide for developers working directly with the source code in this repository.

## Introduction

This repository contains the source code for MediaPipe, a framework for building multimodal (e.g., video, audio, time series) applied machine learning pipelines. With MediaPipe, you can build impressive applications that run on a variety of platforms and devices.

## Core Concepts

MediaPipe is built around a few key concepts that work together to create powerful and efficient machine learning pipelines:

*   **Packet**: The basic data flow unit in MediaPipe. A packet is a wrapper around a piece of data, such as a video frame or a detection result, with a timestamp.
*   **Graph**: A MediaPipe pipeline is represented as a graph of nodes, where each node is a `Calculator` or a `Subgraph`. The graph defines the flow of data between the nodes.
*   **Calculator**: The main processing node in a MediaPipe graph. A calculator performs a specific task, such as image processing, feature extraction, or model inference.
*   **Stream**: A sequence of packets with the same data type. Streams are the "wires" that connect the nodes in a MediaPipe graph.

## Repository Structure

This repository is organized into several key directories:

*   `mediapipe/`: The core C++ source code for the MediaPipe framework, calculators, and solutions.
    *   `framework/`: The core components of the MediaPipe framework, such as the graph, scheduler, and packet.
    *   `calculators/`: A collection of pre-built calculators for various tasks, such as image processing, audio processing, and machine learning inference.
    *   `tasks/`: Cross-platform APIs and libraries for deploying solutions.
    *   `python/`: The Python bindings for MediaPipe, allowing you to build and run MediaPipe graphs in Python.
    *   `java/`: The Java bindings for MediaPipe, for use in Android applications.
    *   `objc/`: The Objective-C bindings for MediaPipe, for use in iOS applications.
    *   `web/`: The JavaScript/TypeScript bindings for MediaPipe, for use in web applications.
*   `docs/`: Documentation for MediaPipe.
*   `examples/`: Example applications for various platforms, demonstrating how to use MediaPipe.

## Getting Started

To get started with MediaPipe, you can explore the example applications in the `mediapipe/examples/` directory. These examples cover a wide range of use cases and platforms.

For detailed instructions on how to use MediaPipe on your platform of choice, please refer to the official documentation:

*   [Python](https://developers.google.com/mediapipe/solutions/setup_python)
*   [Android](https://developers.google.com/mediapipe/solutions/setup_android)
*   [iOS](https://developers.google.com/mediapipe/framework/getting_started/ios)
*   [C++](https://developers.google.com/mediapipe/framework/getting_started/cpp)
*   [Web](https://developers.google.com/mediapipe/solutions/setup_web)

## Building from Source

MediaPipe uses the [Bazel](https://bazel.build/) build system. To build MediaPipe from source, you will need to install Bazel and the required dependencies for your platform. For detailed instructions, please refer to the [installation guide](https://developers.google.com/mediapipe/framework/getting_started/install).

## Contributing

We welcome contributions to MediaPipe. Please follow the [contribution guidelines](https://github.com/google/mediapipe/blob/master/CONTRIBUTING.md) to get started.

## Community

*   [Slack community](https://mediapipe.page.link/joinslack) for MediaPipe users.
*   [Discuss](https://groups.google.com/forum/#!forum/mediapipe) - General community discussion around MediaPipe.
*   [Awesome MediaPipe](https://mediapipe.page.link/awesome-mediapipe) - A curated list of awesome MediaPipe related frameworks, libraries and software.

## Resources

For more information on MediaPipe, including publications and videos, please refer to the [Resources](https://developers.google.com/mediapipe/resources) section on the official MediaPipe website.
