#include "minirt/minirt.h"
#include <cmath>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <stdexcept>

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

void func(Image &image, int elem, int blockSize, int resolutionY, int numOfSamples, ViewPlane viewPlane, Scene scene) {
    // compute pixel's color for rows from index * blockSize to the same + blockSize and assign value to the pixel of image
    for(int x = elem*blockSize; x < (elem*blockSize + blockSize); x++) for (int y = 0; y < resolutionY; y++) {
        const auto color = viewPlane.computePixel(scene, x, y, numOfSamples);
	    image.set(x, y, color);
    }
}

void thread_func(int id, queue<int> &data, mutex &mut, condition_variable &cond, Image &image, int blockSize, int resolutionY, int numOfSamples, ViewPlane viewPlane, Scene scene) {
    while (true) {
    // initialize value of row index
	int elem;
	{
        // wait until queue will be filled then get the first element and delete it in queue
	    unique_lock<std::mutex> lock(mut);
	    while (data.empty()) {
	    	cond.wait(lock);
	    }
	    elem = data.front();
	    data.pop();
	}

    // stop thread when queue is empty
	if (elem == -1) {
	    return;
	}

    // call the function
	func(std::ref(image), elem, blockSize, resolutionY, numOfSamples, viewPlane, scene);
    }
}

int main(int argc, char **argv) {
    // parse arguments from command line
    int viewPlaneResolutionX = (argc > 1 ? std::stoi(argv[1]) : 600);
    int viewPlaneResolutionY = (argc > 2 ? std::stoi(argv[2]) : 600);
    int numOfSamples = (argc > 3 ? std::stoi(argv[3]) : 1);
    int numThreads = (argc > 4 ? std::stoi(argv[4]) : 1);
    int blockSize = (argc > 5 ? std::stoi(argv[5]) : 1);

    // check absence of rest rows
    try {
	if (viewPlaneResolutionX % blockSize != 0) {
	    throw std::invalid_argument("Module of resolutionX by block size must equals 0");
	}
    }
    catch(std::invalid_argument& e) {
	cerr << e.what() << endl;
	return -1;
    }

    // init scene
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
    queue<int> data; // queue for row numbers
    vector<thread> threads; // array for threads
    mutex mut;
    condition_variable cond;
    auto ts = hrc::now(); // timer
    for(int threadNum = 0; threadNum < numThreads; threadNum++) { // init threads
	    thread thread(&thread_func, threadNum, std::ref(data), std::ref(mut), std::ref(cond), std::ref(image), blockSize, viewPlaneResolutionY, numOfSamples, viewPlane, scene);
	    threads.push_back(move(thread)); // add thread to the array
    }

    // fill queue by row indecies
    for(int i = 0; i < viewPlaneResolutionX / blockSize; i++) {
    	{
            lock_guard<mutex> lock(mut);
            data.push(i);
            cond.notify_one();
	    }
    }

    // add value which indicate end of queue
    for(int i = 0; i < numThreads; i++) {
    	lock_guard<mutex> lock(mut);
	    data.push(-1);
    }
    cond.notify_all();

    // stop main thread until all initialized threads are completed
    for(int i = 0; i < numThreads; i++) {
	    threads[i].join();
    }

    auto te = hrc::now(); // timer
    double time = duration<double>(te - ts).count(); // calculate time of complete
    cout << "Time = " << time << endl;
    image.saveJPEG("raytracing.jpg");

    return 0;
}
