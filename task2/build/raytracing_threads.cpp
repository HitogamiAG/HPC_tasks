#include "minirt/minirt.h"
#include <cmath>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>

using namespace std;
using namespace std::chrono;
using namespace minirt;
using hrc = high_resolution_clock;

void initScene(Scene &scene) {
    Color red {1, 0.2, 0.2};
    Color blue {0.2, 0.2, 1};
    Color green {0.2, 1, 0.2};
    Color white {0.8, 0.8, 0.8};
    Color yellow {1, 1, 0.2};

    Material metallicRed {red, white, 50};
    Material mirrorBlack {Color {0.0}, Color {0.9}, 1000};
    Material matteWhite {Color {0.7}, Color {0.3}, 1};
    Material metallicYellow {yellow, white, 250};
    Material greenishGreen {green, 0.5, 0.5};

    Material transparentGreen {green, 0.8, 0.2};
    transparentGreen.makeTransparent(1.0, 1.03);
    Material transparentBlue {blue, 0.4, 0.6};
    transparentBlue.makeTransparent(0.9, 0.7);

    scene.addSphere(Sphere {{0, -2, 7}, 1, transparentBlue});
    scene.addSphere(Sphere {{-3, 2, 11}, 2, metallicRed});
    scene.addSphere(Sphere {{0, 2, 8}, 1, mirrorBlack});
    scene.addSphere(Sphere {{1.5, -0.5, 7}, 1, transparentGreen});
    scene.addSphere(Sphere {{-2, -1, 6}, 0.7, metallicYellow});
    scene.addSphere(Sphere {{2.2, 0.5, 9}, 1.2, matteWhite});
    scene.addSphere(Sphere {{4, -1, 10}, 0.7, metallicRed});

    scene.addLight(PointLight {{-15, 0, -15}, white});
    scene.addLight(PointLight {{1, 1, 0}, blue});
    scene.addLight(PointLight {{0, -10, 6}, red});

    scene.setBackground({0.05, 0.05, 0.08});
    scene.setAmbient({0.1, 0.1, 0.1});
    scene.setRecursionLimit(20);

    scene.setCamera(Camera {{0, 0, -20}, {0, 0, 0}});
}

void thread_func(Image &image, int start, int end, int resolutionY, int numOfSamples, ViewPlane viewPlane, Scene scene) {
    // compute pixel's color for rows from <start> to <end> and assign value to the pixel of image
    for(int x = start; x < end; x++)
    for (int y = 0; y < resolutionY; y++) {
        const auto color = viewPlane.computePixel(scene, x, y, numOfSamples);
	image.set(x, y, color);
    }
}

int main(int argc, char **argv) {
    // parse arguments from command line
    int viewPlaneResolutionX = (argc > 1 ? std::stoi(argv[1]) : 600);
    int viewPlaneResolutionY = (argc > 2 ? std::stoi(argv[2]) : 600);
    int numOfSamples = (argc > 3 ? std::stoi(argv[3]) : 1);
    int numThreads = (argc > 4 ? std::stoi(argv[4]) : 1);

    // if scene is not specified then initialize new one
    Scene scene;
    initScene(scene);
    
    // define some parameters for generated image
    const double backgroundSizeX = 4;
    const double backgroundSizeY = 4;
    const double backgroundDistance = 15;

    const double viewPlaneDistance = 5;
    const double viewPlaneSizeX = backgroundSizeX * viewPlaneDistance / backgroundDistance;
    const double viewPlaneSizeY = backgroundSizeY * viewPlaneDistance / backgroundDistance;

    // compute image
    ViewPlane viewPlane {viewPlaneResolutionX, viewPlaneResolutionY,
                         viewPlaneSizeX, viewPlaneSizeY, viewPlaneDistance};

    Image image(viewPlaneResolutionX, viewPlaneResolutionY); // computed image
    
    // calculate block size
    const double rowsPerThread = viewPlaneResolutionX / numThreads;

    vector<thread> threads; // array for threads
    auto ts = hrc::now(); // timer
    for(int threadNum = 0; threadNum < numThreads; threadNum++) { // init thread with block parameters
        int start = threadNum * rowsPerThread;
	int end = (threadNum + 1) * rowsPerThread;
	thread thread(&thread_func, std::ref(image), start, end, viewPlaneResolutionY, numOfSamples, viewPlane, scene);
	threads.push_back(move(thread)); // add thread to the array
    }
    
    // stop main thread until all initialized threads are completed
    for(int i = 0; i < numThreads; i++) {
	threads[i].join();
    }
    
    // calculate colors for rest pixels (if resolutionX % numThreads != 0)
    for(int x = (numThreads * rowsPerThread); x < viewPlaneResolutionX; x++)
    for(int y = 0; y < viewPlaneResolutionY; y++) {
        const auto color = viewPlane.computePixel(scene, x, y, numOfSamples);
        image.set(x, y, color);
    }
    auto te = hrc::now(); // timer
    double time = duration<double>(te - ts).count(); // calculate time of complete
    cout << "Time = " << time << endl;
    image.saveJPEG("raytracing.jpg");

    return 0;
}
