// Opciones de compilación y ejecución
// g++ $(pkg-config --cflags --libs opencv4) -std=c++11 main.cpp -o GAMMA
// ./GAMMA [-m1 | -m2] -i image gamma [-f x y w h] [-c r g b]
// ./GAMMA [-m1 | -m2] -v gamma [-f x y w h] [-c r g b]

#include <stdio.h>
#include <math.h> 
#include <time.h>
#include <chrono> 
#include <iostream>
#include <opencv2/opencv.hpp>
#include <opencv2/core.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>

using namespace std;
using namespace cv;
using namespace chrono; 

bool timer = true;
const int borde = 50;

// Función de creción de tabla Gamma.
void GammaCorrection(Mat& src, Mat& dst, float fGamma){
	unsigned char lut[256];

	for (int i = 0; i < 256; i++){
		lut[i] = saturate_cast<uchar>(pow((float)(i / 255.0), fGamma) * 255.0f);
	}

	dst = src.clone();
	const int channels = dst.channels();
	switch (channels){

		case 1:{
			MatIterator_<uchar> it, end;
			for (it = dst.begin<uchar>(), end = dst.end<uchar>(); it != end; it++)
				*it = lut[(*it)];
			break;
		}

		case 3:{
			MatIterator_<Vec3b> it, end;
			for (it = dst.begin<Vec3b>(), end = dst.end<Vec3b>(); it != end; it++){
				(*it)[0] = lut[((*it)[0])];
				(*it)[1] = lut[((*it)[1])];
				(*it)[2] = lut[((*it)[2])];
			}
			break;
		}
	}
}


// Función de corrección por tabla.
void corregir_tabla(Mat& img, float gamma_value, int X, int Y, int W, int H, int R, int G, int B){

    cv::Mat M_YUV, M_YUV_Gamma, img_out, img_border;
    vector<cv::Mat> planes;

    // Se convierte la imagen de espacio de color BGR a YUV.
    cv::cvtColor(img, M_YUV, cv::COLOR_BGR2YCrCb);
    cv::split(M_YUV, planes);

    // Se inicia el timer.	
    auto start = high_resolution_clock::now();

   	// Se corrige la capa de luminancia
    GammaCorrection(planes[0], planes[0], gamma_value);

    // Se detiene el timer.
    auto stop = high_resolution_clock::now(); 
    auto duration = duration_cast<microseconds>(stop - start);
    if(timer) cout<<"Tiempo de conversión de imagen por tabla: "<<duration.count()<<" microsegundos"<< endl; 

    // Se mezclan las capas en una nueva imagen corregida.
    cv::merge(planes, M_YUV_Gamma);

    // Se convierte la imagen de espacio de color YUB a BGR.
    cv::cvtColor(M_YUV_Gamma, img_out, cv::COLOR_YCrCb2BGR);

    // Se recorta el rectángulo a mostrar.
    cv::Mat whole = img_out; // Imagen original.
    cv::Mat part(
    whole,
    cv::Range( Y, Y+H ), // rows.
    cv::Range( X, X+W ));// cols.
    part.copyTo(img(cv::Rect(X, Y, part.cols, part.rows)));

    // Se genera el borde.
    Scalar value(R,G,B);
    copyMakeBorder(img, img_border, borde, borde, borde, borde, BORDER_CONSTANT, value);

    // Se muestra la imagen corregida.
    cv::imshow("Imagen con correcion Gamma por tabla en capa de luminancia", img_border);
} 


// Función de correción pixel a pixel.
void corregir_pixel(Mat& img, float gamma_value, int X, int Y, int W, int H, int R, int G, int B){
	
    cv::Mat M_YUV, img_out, img_border, img_aux;
    vector<cv::Mat> planes, canales;
    img_aux = img;

    // Se convierte la imagen de espacio de color BGR a YUV.
    cv::cvtColor(img_aux, M_YUV, cv::COLOR_BGR2YCrCb);
    cv::split(M_YUV, planes);
    
    // Se extraen los valores de las imágenes
    uchar *data_old = M_YUV.data;
    uchar *data_new = img_aux.data;
    
    int i, j, cols = img_aux.cols, rows = img_aux.rows;

    // Se inicia el timer	
    auto start = high_resolution_clock::now();

    // Se corrige la capa de luminancia pixel a pixel.
    for (i = 0; i < rows*3; i+=3){
        for (j = 0; j < cols*3; j+=3){
        	data_new[i*cols+j]= saturate_cast<uchar>(pow((float)(data_old[i*cols+j]/255.0), gamma_value) * 255.0f);
        }
    }

    // Se detiene el timer
    auto stop = high_resolution_clock::now(); 
    auto duration = duration_cast<microseconds>(stop - start);
    if(timer) cout << "Tiempo de conversión de imagen pixel a pixel: "<< duration.count() << " microsegundos" << endl; 

    // Se compone la nueva imagen a partir del canal de luminancia corregido en gamma junto a los canales de croma originales.
    cv::split(img_aux, canales);
    canales = {canales[0],planes[1],planes[2]};
    cv::merge(canales, img_out);

    // Se convierte la imagen de espacio de color YUV a BGR.
    cv::cvtColor(img_out, img_out, cv::COLOR_YCrCb2BGR);
    cv::cvtColor(M_YUV, img, cv::COLOR_YCrCb2BGR);

    // Se recorta el rectángulo a mostrar.
    cv::Mat whole = img_out; // Imagen original.
    cv::Mat part(
    whole,
    cv::Range( Y, Y+H ), // rows.
    cv::Range( X, X+W ));// cols.
    part.copyTo(img(cv::Rect(X, Y, part.cols, part.rows)));

    // Se genera el borde.
    Scalar value(R,G,B);
    copyMakeBorder(img, img_border, borde, borde, borde, borde, BORDER_CONSTANT, value);

    // Se muestra la imagen corregida.
    cv::imshow("Imagen con correcion Gamma por funcion en capa de luminancia", img_border);
}

int main(int argc, char *argv[]){

    // Valores booleanos que determinan el tipo de procesamiento a realizar.
	bool tabla = false, pixel = false, imagen = false, video = false;
    int R = 0, G = 0, B = 0;
    int X = 0, Y = 0, W, H;
	
    // Muestra mensaje error en caso de utilizar menos de 4 argumentos o más de 6.
	if(argc < 4) {
        cerr << "Usage: ./GAMMA [-m1 | -m2] -i image gamma [-f x y w h] [-c r g b]" << endl;
        cerr << "Usage: ./GAMMA [-m1 | -m2] -v gamma [-f x y w h] [-c r g b]" << endl;
        return 1;
    }

    // Encuentra el valor "i" en el segundo argumento.
    if ((argv[2][1])==105 || (argv[2][1])==73){
    	imagen = true;
    	cout<<"Procesamiento de imagen ";
    }

    // Encuentra el valor "v" en el segundo argumento.
    if ((argv[2][1])==118 || (argv[2][1])==86){
    	video = true;
    	cout<<"Procesamiento de video ";
    }

    // Encuentra el valor "-m1" en el primer argumento.
    if ((argv[1][2])==49){
    	tabla = true;
		cout<<"por tabla, ";
	}

    // Encuentra el valor "-m2" en el primer argumento.
    if ((argv[1][2])==50){
    	pixel = true;
    	cout<<"pixel a pixel, ";
    }


    cv::Mat img, img_out;
    float gamma_value;

    // Lógica de procesamiento de imagen.
    if (imagen){
        // Lee la imagen
    	img = cv::imread(argv[3], 1);
        R = G = B = X = Y = 0;
        W = img.cols;
        H = img.rows;

        // Si no se encuentra la imagen. 
	    if(img.empty()) {
	        cerr << "Error leyendo imagen " << argv[3] << endl;
	        return 1;
	    }

        // Guarda el valor de Gamma
	    gamma_value = atof(argv[4]);
    	cout<<"nivel gamma : "<<gamma_value<<endl;
 

        if(argc == 9){ // Solo definición de borde
            if ((argv[5][1])==99 || (argv[5][1])==67){ // c ó C
                R = atof(argv[8]);
                G = atof(argv[7]);
                B = atof(argv[6]);
            }
        }

        if(argc == 10){ // Solo definición de rectángulo
            if ((argv[5][1])==102 || (argv[5][1])==70){ // f ó F
                X = atof(argv[6]);
                Y = atof(argv[7]);
                W = atof(argv[8]);
                H = atof(argv[9]);
            }
        }

        if(argc>10){ // Ambas definiciones
            if ((argv[5][1])==102 || (argv[5][1])==70){ // f ó F
                X = atof(argv[6]);
                Y = atof(argv[7]);
                W = atof(argv[8]);
                H = atof(argv[9]);
            }
            if ((argv[10][1])==99 || (argv[10][1])==67){ // c ó C
                R = atof(argv[13]);
                G = atof(argv[12]);
                B = atof(argv[11]);
            }   
        }
        
        cout << "Presionar cualquier tecla para salir" << endl;

        // Corrección Gamma con tabla.
        if (tabla) corregir_tabla(img, gamma_value, X, Y, W, H, R, G, B);
        
        // Corrección Gamma con función.
        if (pixel) corregir_pixel(img, gamma_value, X, Y, W, H, R, G, B); 
            
    }
        


    // Lógica de procesamiento de video.
    if (video){
        gamma_value = atof(argv[3]);
        cout<<"nivel gamma : "<<gamma_value<<endl;

        Mat frame;
        // Captura de video.
        VideoCapture cap;
        // Abre la cámara default usando la API por default.
        // cap.open(0);
        int deviceID = 0;             // 0 = Cámara por default.
        int apiID = cv::CAP_ANY;      // 0 = Autodetectar API por default.
        // Abrir la cámara seleccionada usando la API seleccionada.
        cap.open(deviceID, apiID);

        // Revisar si no hay errores
        if (!cap.isOpened()) {
            cerr << "Error abriendo camara\n";
            return -1;
        }
        
        // Loop de captura.
        cout << "Se inicia la grabacion" << endl
        << "Presionar cualquier tecla para salir" << endl;
        for (;;){
            // Esperar por u nuevo frame.
            cap.read(frame);
            R = G = B = X = Y = 0;
            W = frame.cols;//1280;
            H = frame.rows;//720;

            // Revisar si no hay errores.
            if (frame.empty()) {
                cerr << "Frame en blanco\n";
                break;
            }
	
            if(argc == 8){ // Solo definición de borde
                if ((argv[4][1])==99 || (argv[4][1])==67){ // c ó C
                    R = atof(argv[7]);
                    G = atof(argv[6]);
                    B = atof(argv[5]);
                }
            }

            if(argc == 9){ // Solo definición de rectángulo
                if ((argv[4][1])==102 || (argv[4][1])==70){ // f ó F
                    X = atof(argv[5]);
                    Y = atof(argv[6]);
                    W = atof(argv[7]);
                    H = atof(argv[8]);
                } 
            }

            else if(argc>9){ // Ambas definiciones
                if ((argv[4][1])==102 || (argv[4][1])==70){ // f ó F
                    X = atof(argv[5]);
                    Y = atof(argv[6]);
                    W = atof(argv[7]);
                    H = atof(argv[8]);
                }   
                if ((argv[9][1])==99 || (argv[9][1])==67){ // c ó C
                    R = atof(argv[12]);
                    G = atof(argv[11]);
                    B = atof(argv[10]);
                }
            }

            // Corrección Gamma con tabla.
            if (tabla) corregir_tabla(frame, gamma_value, X, Y, W, H, R, G, B);

            // Corrección Gamma con función.
            if (pixel) corregir_pixel(frame, gamma_value, X, Y, W, H, R, G, B);

            if (waitKey(5) >= 0)
                break;  
        }
        return 0;
    }
    waitKey(0);
    return 0;
}






