#include <iostream>
#include <pthread.h>
#include <vector>
#include <cstdlib>
#include <fstream>
#include <algorithm>
using namespace std;

//variables for program use
int seed;
int numberOfPlayers;
int chipBagSize;
ofstream logFile;


//game variables
int greasyCard;
vector<int> initialDeck = {
        // each round starts with a full standard deck
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    };
vector<int> deck;
vector<int> playerHand;
int chipsInBag;
long long currentRound;     //needed to be long long for size type casting
int currentPlayer;
int currentDealer;
int winner;
bool checkRoundEnded;


//mutexes for data syncronization
pthread_mutex_t deckMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t chipBagMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writeToFileMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t writeToConsoleMutex = PTHREAD_MUTEX_INITIALIZER;

string printCard(int cardToPrint){
    string cardName;
    if (cardToPrint != 1 && cardToPrint < 11){
        cardName = to_string(cardToPrint);
    }
    else if(cardToPrint == 1){ cardName = "Ace";}

    else if(cardToPrint == 11){cardName = "Jack";}

    else if(cardToPrint == 12){cardName = "Queen";}

    else{cardName = "King";}
    return cardName;
}


void printDeck()
{
    logFile << "Cards in Deck: ";
    for(int deckCounter = 0; deckCounter < deck.size(); ++deckCounter)
    {
        logFile << printCard(deck[deckCounter]) << " ";
    }
    logFile << endl;
}

//Sets the Dealers Behavior to a specified function: Takes PlayerID from creation as its argument
void* dealer(void* arg){
    //pull threadID from creation and establish dealer thread number
    //set to current Dealer
    int dealerNumber = (long long)arg;
    currentDealer = dealerNumber;

    //Lock: Critical Section
    pthread_mutex_lock(&deckMutex);
    deck = initialDeck;
    // shuffle the deck
    random_shuffle(deck.begin(), deck.end());
    //cout << "Deck is being Shuffled \n";
    greasyCard = deck.back();
    deck.pop_back();
    pthread_mutex_unlock(&deckMutex);

    playerHand.clear();
    playerHand.resize(numberOfPlayers + 1);

    for(int playerCounter = 1; playerCounter <= numberOfPlayers; playerCounter++){
        if(playerCounter != dealerNumber){
            playerHand[playerCounter] = deck.back();
            deck.pop_back();
        }
    }
    //critical section for writing out to console and logging file information
    pthread_mutex_lock(&writeToFileMutex);
    logFile << "ROUND " << currentRound << ": Greasy Card is " << printCard(greasyCard) << endl;
    cout << "ROUND " << currentRound << ": Greasy Card is " << printCard(greasyCard) << endl;
    pthread_mutex_unlock(&writeToFileMutex);

    //critical section regarding chip bag
    pthread_mutex_lock(&chipBagMutex);
    chipsInBag = chipBagSize;
    logFile << "BAG: " << chipsInBag << " Chips left" << endl;
    pthread_mutex_unlock(&chipBagMutex);

    currentPlayer = (dealerNumber % numberOfPlayers) + 1;
    checkRoundEnded = false;
    winner = 0;
    return NULL;
}

void* player(void* arg){

    //Initialization of args to establish playerID and player's current Hand
    int playerNumber = (long long) arg;
    vector<int> playerCurrentHand;
    playerCurrentHand.push_back(playerHand[playerNumber]);

    while(true)
    {
        pthread_mutex_lock(&writeToConsoleMutex);
        if(checkRoundEnded) {
            pthread_mutex_unlock(&writeToConsoleMutex);
            break;
        }
        pthread_mutex_unlock(&writeToConsoleMutex);

        if(currentPlayer == playerNumber)
        {
            pthread_mutex_lock(&deckMutex);
            logFile << "PLAYER " << playerNumber << ": hand " << playerCurrentHand[0] << endl;
            playerCurrentHand.push_back(deck.back());
            logFile << "PLAYER " << playerNumber << ": draws " << deck.back() << endl;
            deck.pop_back();
            printDeck();
			//compare hand to greasy card logic
            if(playerCurrentHand[0] == greasyCard || playerCurrentHand[1] == greasyCard)
            {
                pthread_mutex_lock(&writeToConsoleMutex);
                logFile << "PLAYER " << playerNumber << ": hand (" << playerCurrentHand[0] << ", " << playerCurrentHand[1] << ") <> Greasy card is " << greasyCard << endl;
                currentPlayer = 0;
                winner = playerNumber;
                checkRoundEnded = true;
                pthread_mutex_unlock(&writeToConsoleMutex);
            }
            else
            {
				//discard card logic 
				int sizeOfCurHand = 2;
                int randomIndex = rand() % sizeOfCurHand;
                int discard = playerCurrentHand[randomIndex];
                playerCurrentHand.erase(playerCurrentHand.begin() + randomIndex);
                logFile << "PLAYER " << playerNumber << ": discards " << discard << " at random" << endl;
                deck.insert(deck.begin(), discard);
                printDeck();
				//chip eating logic
                pthread_mutex_lock(&chipBagMutex);
                logFile << "BAG: " << chipsInBag << " Chips left" << endl;
                int chips_eaten = (rand() % 5) + 1;
                logFile << "PLAYER " << playerNumber << ": eats " << chips_eaten <<  " chips" << endl;
                chipsInBag -= chips_eaten;
                if(chipsInBag <= 0) {
                    chipsInBag += chipBagSize;
                }
                logFile << "BAG: " << chipsInBag << " Chips left" << endl;
                pthread_mutex_unlock(&chipBagMutex);
            }
            currentPlayer = (currentPlayer % numberOfPlayers) + 1;
            if(currentPlayer == currentDealer) {
                if(currentPlayer == numberOfPlayers) {currentPlayer = 1;}
                else {++currentPlayer;}
            }
            logFile << "Next player: " << currentPlayer << endl;
            pthread_mutex_unlock(&deckMutex);
        }
    }
    pthread_mutex_lock(&writeToFileMutex);
    if(playerNumber == winner) {
        logFile << "PLAYER " << playerNumber << ": wins round " << currentRound << endl;
        cout << "PLAYER " << playerNumber << ": wins round " << currentRound << endl;
    }
    else {
        logFile << "PLAYER " << playerNumber << ": lost round " << currentRound << endl;
        cout << "PLAYER " << playerNumber << ": lost round " << currentRound << endl;
    }
    pthread_mutex_unlock(&writeToFileMutex);
    return NULL;
}

int main(int argc, char* argv[])
{
    //Check to evaluate correct number of arguments
    if (argc != 4) {
        cout << "ERROR: Missing Arguments. Please supply the following arguments in the following format {executable file} <seed> <numberofplayers> <chipbagsize>." << endl;
        exit(1);
    }
    //Set passed values to their appropriate global value
    seed = atoi(argv[1]);
    numberOfPlayers = atoi(argv[2]);
    chipBagSize = atoi(argv[3]);

    //seed rand and open log file
    srand(seed);
    logFile.open("log.txt");

    // Create threads for dealer and players
    pthread_mutex_init(&deckMutex, NULL);
    pthread_mutex_init(&chipBagMutex, NULL);
    pthread_mutex_init(&writeToFileMutex, NULL);
    pthread_t dealerThread;
    pthread_t playerThreads[numberOfPlayers + 1];

    for (currentRound = 1; currentRound <= numberOfPlayers; currentRound++) {
        //create dealer thread and subsequently join when work is finished
        pthread_create(&dealerThread, NULL, dealer, (void*)currentRound);
        pthread_join(dealerThread, NULL);
        //Creation of subsequent player threads, making sure not to create the dealer
        for (long long threadID = 1; threadID <= numberOfPlayers; ++threadID) {
            if(threadID != currentRound) {
                //cout << "thread: " << threadID << " was created. \n";
                pthread_create(&playerThreads[threadID], NULL, player, (void*)threadID);
            }
        }
        // Wait for player threads to finish
        for (int threadCount = 1; threadCount <= numberOfPlayers; ++threadCount) {
            if(threadCount != currentRound) {
                //cout << "player thread: " << threadID << " was joined. \n";
                pthread_join(playerThreads[threadCount], NULL);
            }
        }
        pthread_mutex_lock(&writeToFileMutex);
        logFile << "The Current Round: " << currentRound << " has ended. \n\n";
        pthread_mutex_unlock(&writeToFileMutex);
    }

    //Destroy Mutexes and free Resources and close File
    pthread_mutex_destroy(&deckMutex);
    pthread_mutex_destroy(&chipBagMutex);
    pthread_mutex_destroy(&writeToFileMutex);

    logFile.close();

    return 0;
}
