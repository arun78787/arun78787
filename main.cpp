#include <iostream>
#include <unistd.h>
#include <CoreAudio/CoreAudio.h>
#include <termios.h>
using namespace std;

// Add this member function to the olcNoiseMaker class declaration
template <class T>
class olcNoiseMaker
{
public:
    // Constructor and other members...

    // SetUserFunction member function declaration
    void SetUserFunction(double (*func)(double));

    // Function to get the current time
    double GetTime();

private:
    // Add this member variable to store the user function
    double (*m_userFunction)(double);
};

// Explicitly instantiate the class for short
template class olcNoiseMaker<short>;

// SetUserFunction member function definition
template <class T>
void olcNoiseMaker<T>::SetUserFunction(double (*func)(double))
{
    m_userFunction = func;
}

// Function to get the current time (dummy implementation)
template <class T>
double olcNoiseMaker<T>::GetTime()
{
    return 0.0; // Replace with the actual implementation
}

bool kbhit()
{
    struct termios oldt, newt;
    int ch;
    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if (ch != EOF)
    {
        ungetc(ch, stdin);
        return true;
    }
    return false;
}

// Global synthesizer variables
atomic<double> dFrequencyOutput = 0.0; // dominant output freq of the instrument, i.e., the note
double dOctaveBaseFrequency = 110.0;   // frequency of the octave represented by the keyboard
double d12thRootOf2 = pow(2.0, 1.0 / 12.0); // assuming western 12 notes per octave

// Function used by olcNoiseMaker to generate sound waves
// Returns amplitude (-1.0 to +1.0) as a function of time
double MakeNoise(double dTime)
{
    double dOutput = sin(dFrequencyOutput * 2.0 * 3.14159 * dTime);
    return dOutput * 0.5; // master volume
}

int main()
{
    // Shameless self-promotion
    wcout << "WWW.OneLoneCoder.com - Synthesizer Part 1" << endl
          << "Single Sine Wave Oscillator, No Polyphony" << endl
          << endl;

    // Display a keyboard
    wcout << endl
          << "|    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |    |" << endl
          << "|    | S  |    |    | F  |    | G  |    |    |  J |    |  K |    | L  |    |    |" << endl
          << "|    |____|    |    |____|    |____|    |    |____|    |____|    |____|    |    |__" << endl
          << "|      |       |       |        |       |       |         |         |      |      |" << endl
          << "|  Z   |   X   |   C   |   V    |   B   |   N   |    M    |    ,    |  .   |  /   |" << endl
          << "|______|_______|_______|________|_______|_______|_________|_________|______|______|" << endl
          << endl;

    // Create sound machine
    olcNoiseMaker<short> sound;

    // Link noise function with sound machine
    sound.SetUserFunction(MakeNoise);

    // Sit in loop, capturing keyboard state changes & modify
    // synthesizer output accordingly
    int nCurrentKey = -1;
    bool bKeyPressed = false;
    while (1)
    {
        bKeyPressed = false;
        for (int k = 0; k < 16; k++)
        {
            if (kbhit()) // use kbhit func for key detection
            {
                if (nCurrentKey != k)
                {
                    dFrequencyOutput = dOctaveBaseFrequency * pow(d12thRootOf2, k);
                    wcout << "\rNote On : " << sound.GetTime() << "s " << dFrequencyOutput << "Hz";
                    nCurrentKey = k;
                }

                bKeyPressed = true;
            }
        }

        if (!bKeyPressed)
        {
            if (nCurrentKey != -1)
            {
                wcout << "\rNote Off: " << sound.GetTime() << "s                   ";
                nCurrentKey = -1;
            }
            dFrequencyOutput = 0.0;
        }
    }
    return 0;
}
