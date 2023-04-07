#include <cstdlib>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <cstdio>
#include <sys/types.h>
#include <pthread.h>
#include <fcntl.h>
#include <vector>
#include <time.h>
#include <thread>

const int maxSize = 4096;

    size_t a;
    size_t c;
    size_t m;
    size_t seed;
    char* iFilePath;
    char* oFilePath;

struct lkgGenParam
{
    size_t a;
    size_t c;
    size_t m;
    size_t seed;
    size_t sizeKey;
};

struct thread
{
    char* msg;
    char* random_subsequence;
    char* outputText;
    size_t size;
    size_t bottomIndex;
    size_t topIndex;
    pthread_barrier_t* barrier;
};

void clearMemory(const char* outputText, const char* msg, const char* random_subsequence)
{
    delete[] outputText;
    delete[] msg;

    if (random_subsequence != nullptr)
        delete[] random_subsequence;
}

void* lkg(void* lkgParam)
{
    lkgGenParam *arguments = reinterpret_cast<lkgGenParam *>(lkgParam);
    size_t a = arguments->a;
    size_t m = arguments->m;
    size_t c = arguments->c;
    size_t sizeKey = arguments->sizeKey;

    int* buff = new int [sizeKey / sizeof(int) + 1];
    buff[0] = arguments->seed;

    for(size_t i = 1; i < sizeKey / sizeof(int) + 1; i++)
    {
        buff[i]= (a * buff[i-1] + c) % m;
    }

    return reinterpret_cast<char *>(buff);
}

void* crypt(void * cryptParametrs)
{
    thread* param = reinterpret_cast<thread*>(cryptParametrs);
    size_t topIndex = param->topIndex;
    size_t bottomIndex = param->bottomIndex;

    while(bottomIndex < topIndex)
    {
        param->outputText[bottomIndex] = param->random_subsequence[bottomIndex] ^ param->msg[bottomIndex];
        bottomIndex++;
    }

    int status = pthread_barrier_wait(param->barrier);
    if (status != 0 && status != PTHREAD_BARRIER_SERIAL_THREAD) {
        exit(status);
    }

    return nullptr;
}



int main (int argc, char **argv)
{
    if (argc != 13)
    {
        std::__throw_invalid_argument("Argument error");
    }


    int c;
    while ((c = getopt(argc, argv, "i:o:a:c:x:m:")) != -1)
    {
        switch (c)
        {
            case 'i':
                {
                    iFilePath = optarg;
                    break;
                }
            case 'o':
                {
                    oFilePath = optarg;
                    break;
                }
            case 'a':
                {
                    a = atoi(optarg);
                    break;
                }
            case 'c':
                {
                    c = atoi(optarg);
                    break;
                }
            case 'm':
                {
                    m = atoi(optarg);
                    break;
                }
            case 'x':
                {
                    seed = atoi(optarg);
                    break;
                }
            case '?':
                break;
            default:
                std::__throw_invalid_argument("Argument error");
        }
    }

    if (optind < argc) {
        std::__throw_invalid_argument("Argument error");
    }


    int inputFile = open(iFilePath, O_RDONLY);
    if (inputFile == -1)
    {
        std::__throw_invalid_argument("Unable to open input file");
    }

    int inputSize = lseek(inputFile, 0, SEEK_END);
    std::cout << "input file size = " << inputSize << std::endl;
    if(inputSize == -1)
    {
        std::__throw_invalid_argument("Unable to get size");
    }

    if (inputSize == 0)
    {
        std::__throw_invalid_argument("File is empty");
}

    if (inputSize > maxSize)
    {
        std::__throw_invalid_argument("File is too large");
    }


    char* randomSubsequence = nullptr;
    char* currentText = new char[inputSize];
    char* msg = new char[inputSize];

    lseek(inputFile, 0, SEEK_SET);

    if(read(inputFile, msg, inputSize) == -1)
    {
        clearMemory(currentText, msg, randomSubsequence);
        std::__throw_invalid_argument("Cant read file");
    }

    unsigned int countProcessors = std::thread::hardware_concurrency();;
    std:: cout << "Processors: " << countProcessors << std:: endl;

    lkgGenParam lkgParam;
    lkgParam.sizeKey = inputSize ;
    lkgParam.a=a;
    lkgParam.c=c;
    lkgParam.m=m;
    lkgParam.seed=seed;

    pthread_t keyGenThread;
    pthread_t cryptThread[countProcessors];

    if (pthread_create(&keyGenThread, NULL, lkg, &lkgParam) != 0)
    {
        clearMemory(currentText, msg, randomSubsequence);
        std::__throw_invalid_argument("Unable to create a new thread");
    }

    size_t random_subsequence_thread_status = pthread_join(keyGenThread, (void**)&randomSubsequence);
    if(random_subsequence_thread_status != 0)
    {
        clearMemory(currentText, msg, randomSubsequence);
        std::__throw_invalid_argument("Unable to join random subsequence thread");
    }

    std::vector <thread*> threads;


    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, countProcessors + 1);


    for(unsigned int i = 0; i < countProcessors; i++)
    {
        thread* threadParams = new thread;

        threadParams->random_subsequence = randomSubsequence;
        threadParams->size = inputSize;
        threadParams->outputText = currentText;
        threadParams->msg = msg;
        threadParams->barrier = &barrier;

        size_t currentLength = inputSize / countProcessors;

        threadParams->bottomIndex = i * currentLength;
        threadParams->topIndex = i * currentLength + currentLength;

        if (i == countProcessors - 1)
        {
            threadParams->topIndex = inputSize;
        }

        threads.push_back(threadParams);
        pthread_create(&cryptThread[i], NULL, crypt, threadParams);
    }

    int status = pthread_barrier_wait(&barrier);

    if (status != 0 && status != PTHREAD_BARRIER_SERIAL_THREAD)
    {
        clearMemory(currentText, msg, randomSubsequence);
        for (auto & thread : threads)
        {
            delete thread;
        }
        exit(status);
    }

    int output;
    if ((output=open(oFilePath, O_WRONLY)) == -1)
    {
        std::__throw_invalid_argument("Cannot open file");
    }
    else
    {
        if(write(output, currentText, inputSize) != inputSize)
            std::__throw_invalid_argument("Write error");

        close(output);
    }

    pthread_barrier_destroy(&barrier);


    clearMemory(currentText, msg, randomSubsequence);

    for (auto & t : threads)
    {
        delete t;
    }
    return 0;
}

