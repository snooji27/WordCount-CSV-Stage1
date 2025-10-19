#include <iostream>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <string>
#include <algorithm>
#include <dirent.h>
#include <cctype>
#include <omp.h>  //OpenMP header
#include <iomanip>  //for formatted time output


using namespace std;

//clean a word by removing punctuation, lowercase
string clean_word(const string &word) {
    string result;
    for (char c : word)
        if (isalpha(static_cast<unsigned char>(c)))
            result += tolower(static_cast<unsigned char>(c));//end if
    return result;//end for-loop
}//end clean_word

//count words in one CSV file (sequential)
unordered_map<string, int> count_words_in_file(const string &filename) {
    ifstream file(filename);
    unordered_map<string, int> freq;
    string line;

    //if file opened successfully
    if (!file.is_open()) {
        cerr << "Error: Could not open file: " << filename << endl;
        return freq;//return empty map
    }//end if

    //benchmark start for nested loop (word processing section)
    double loop_start = omp_get_wtime();

    int target_idx = 12; //M1 column is 13th column, after 12 commas

    //skip header line
    getline(file, line);

    //process each CSV row
    while (getline(file, line)) {
        vector<string> columns;
        size_t pos2 = 0;
        string field;
        string temp = line;
        while ((pos2 = temp.find(',')) != string::npos) { //comma-separated CSV
            field = temp.substr(0, pos2);
            columns.push_back(field);
            temp.erase(0, pos2 + 1);
        }
        columns.push_back(temp); //last column

        if ((int)columns.size() > target_idx) {
            string clean = clean_word(columns[target_idx]);
            if (!clean.empty())
                freq[clean]++;//add 1, end if
        }
    }//end while-loop

    double loop_end = omp_get_wtime();
    double loop_elapsed = loop_end - loop_start;
    cout << fixed << setprecision(6)
         << "       Inner loop time for " << filename
         << ": " << loop_elapsed << " seconds\n";

    file.close();//close file after reading
    return freq;//return word count map for current map
}//end counting words in a file sequentially

//merge local maps: adds up the counts from local into global
void merge_maps(unordered_map<string, int> &global, const unordered_map<string, int> &local) {
    for (const auto &p : local)
        global[p.first] += p.second;//end for
}//end merge_maps

int main() {
    cout << "**** Sequential Word Frequency Counter ****\n";

    //show system info (from lecture 06 slides 34 and 52)
    cout << "System Info:\n";
    cout << "  - Available processors: " << omp_get_num_procs() << endl;
    cout << "  - Max available threads: " << omp_get_max_threads() << endl;
    cout << "  - OpenMP version: " << _OPENMP << endl;

    string folder = "data"; //folder with test data in .txt files
    unordered_map<string, int> global_freq; //global_freq to store counts of each words in a table
    int file_count = 0; //initializing the file count

    //benchmark total runtime
    double total_start = omp_get_wtime();

    //use omp_get_wtime() to get the wall clock time in seconds (from lecture 06 slide 25)
    double start_time = omp_get_wtime();

    DIR *dir;
    struct dirent *ent;

    double slowest_time = 0.0;
    string slowest_file = "";

    double total_end = 0.0; //declare here so we can use it outside if-block later
    double total_elapsed = 0.0;

    if ((dir = opendir(folder.c_str())) != NULL) { //open folder/directory
        while ((ent = readdir(dir)) != NULL) {//loop through all files/folders inside the directory
            string filename = ent->d_name; //get name of current file/folder
            if (filename == "." || filename == "..") continue;//skip current & parent, end inner if
            file_count++;//increment num of files found

            string path = folder + "/" + filename; //create full path to current file

            //start timing this file
            double file_start = omp_get_wtime();
            auto local = count_words_in_file(path); //moved here to count words
            double file_end = omp_get_wtime();
            double elapsed = file_end - file_start;

            //find the most frequent word in this file
            if (!local.empty()) {
                int max_freq = 0;
                for (const auto &p : local)
                    if (p.second > max_freq) max_freq = p.second;

                if (max_freq <= 1) {
                    cout << "File: " << filename << " -> No duplicate words.\n";
                } else {
                    vector<string> top_words;
                    for (const auto &p : local)
                        if (p.second == max_freq)
                            top_words.push_back(p.first);

                    sort(top_words.begin(), top_words.end());
                    cout << "File: " << filename << " -> Most frequent word(s) (count: "
                         << max_freq << "): ";
                    for (size_t i = 0; i < top_words.size(); ++i) {
                        cout << top_words[i];
                        if (i != top_words.size() - 1) cout << ", ";
                    }
                    cout << endl;
                }
            }

            //print timing for this file
            cout << "  Execution time for " << filename << ": " << fixed << setprecision(4)
                 << elapsed << " seconds\n";

            //track slowest file
            if (elapsed > slowest_time) {
                slowest_time = elapsed;
                slowest_file = filename;
            }

            merge_maps(global_freq, local);//merge current file count with global word count map
        }//end while-loop
        closedir(dir);//close directory when done
        total_end = omp_get_wtime();
        total_elapsed = total_end - total_start;

    } else {//folder couldn't be opened
        cerr << "Error: Could not open directory: " << folder << endl;
        cerr << "Please create a folder named 'data' and add text files inside it.\n";//solution
        return 1;//exit program with error code
    }//end if-else

    double end_time = omp_get_wtime();
    double elapsed = end_time - start_time;

    cout << "\nProcessed " << file_count << " files." << endl;//num of files
    cout << "Unique words: " << global_freq.size() << endl;//print num of unique/unrepeated words (freq=1)
    cout << "Execution time (omp_get_wtime): " << elapsed << " seconds\n";//print exec time

    //sort by frequency descending (most frequent first)
    vector<pair<string, int>> sorted(global_freq.begin(), global_freq.end());
    sort(sorted.begin(), sorted.end(),
         [](auto &a, auto &b) { return a.second > b.second; });

    cout << "\nTop 10 most frequent words:\n";//print most frequent 10 words
    //if words have same frequency, they're printed in alphabetical order
    for (int i = 0; i < 10 && i < (int)sorted.size(); i++)
        cout << sorted[i].first << " : " << sorted[i].second << endl;//end for-loop

    cout << "\n**** Benchmark Summary ****\n";
    cout << "Total files processed: " << file_count << endl;
    cout << "Unique words overall: " << global_freq.size() << endl;
    cout << "Total execution time: " << fixed << setprecision(4)
         << total_elapsed << " seconds\n";
    cout << "Slowest file: " << slowest_file << " ("
         << fixed << setprecision(4) << slowest_time << " seconds)\n";

    cout << "\nSequential execution completed (OpenMP runtime linked without parallelism yet)\n";

    return 0;
}
