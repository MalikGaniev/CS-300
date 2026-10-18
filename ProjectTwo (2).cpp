// CS 300 Project Two: ABCU Advising Assistance Program
// Build: g++ -std=c++11 -Wall -Wextra -pedantic ProjectTwo.cpp -o ProjectTwo
// CSV format: courseNumber,courseTitle,prerequisite1,prerequisite2,...
#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Course {
    std::string number;
    std::string title;
    std::vector<std::string> prerequisites;
};

std::string trim(const std::string& text) {
    const auto first = text.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
}

std::string normalizeNumber(std::string number) {
    number = trim(number);
    for (char& character : number) {
        character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    }
    return number;
}

bool validNumber(const std::string& number) {
    return !number.empty() && std::all_of(number.begin(), number.end(),
        [](unsigned char character) { return std::isalnum(character) != 0; });
}

// Parse a single CSV record, including quoted titles and escaped quotation marks.
std::vector<std::string> parseCsv(const std::string& line) {
    std::vector<std::string> fields;
    std::size_t position = 0;
    while (true) {
        while (position < line.size() && (line[position] == ' ' || line[position] == '\t')) ++position;
        std::string field;
        if (position < line.size() && line[position] == '"') {
            ++position;
            bool closed = false;
            while (position < line.size()) {
                char character = line[position++];
                if (character == '"') {
                    if (position < line.size() && line[position] == '"') {
                        field += '"';
                        ++position;
                    } else {
                        closed = true;
                        break;
                    }
                } else field += character;
            }
            if (!closed) throw std::runtime_error("Unclosed quoted field.");
            while (position < line.size() && std::isspace(static_cast<unsigned char>(line[position]))) ++position;
            if (position < line.size() && line[position] != ',') {
                throw std::runtime_error("Unexpected text after a quoted field.");
            }
        } else {
            while (position < line.size() && line[position] != ',') {
                if (line[position] == '"') throw std::runtime_error("Unexpected quotation mark.");
                field += line[position++];
            }
        }
        fields.push_back(trim(field));
        if (position == line.size()) break;
        ++position; // Skip the comma; preserve empty fields for validation.
    }
    return fields;
}

class BinarySearchTree {
    struct Node {
        Course course;
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;
        explicit Node(Course value) : course(std::move(value)) {}
    };
    std::unique_ptr<Node> root;
    std::size_t courseCount = 0;

    static void printInOrder(const Node* node) {
        if (!node) return;
        printInOrder(node->left.get());
        std::cout << node->course.number << ", " << node->course.title << '\n';
        printInOrder(node->right.get());
    }

public:
    bool insert(Course course) {
        auto* link = &root;
        while (*link) {
            if (course.number == (*link)->course.number) return false;
            link = course.number < (*link)->course.number ? &(*link)->left : &(*link)->right;
        }
        link->reset(new Node(std::move(course)));
        ++courseCount;
        return true;
    }

    const Course* search(const std::string& number) const {
        const Node* node = root.get();
        while (node) {
            if (number == node->course.number) return &node->course;
            node = number < node->course.number ? node->left.get() : node->right.get();
        }
        return nullptr;
    }

    void printCourses() const { printInOrder(root.get()); }
    std::size_t size() const { return courseCount; }
};

bool loadCourses(const std::string& filename, BinarySearchTree& courses) {
    std::ifstream input(filename);
    if (!input) {
        std::cout << "Error: Could not open file: " << filename << '\n';
        return false;
    }
    // Build a temporary tree so a failed reload preserves the previous data.
    BinarySearchTree loaded;
    std::vector<std::pair<std::string, std::size_t>> references;
    std::string line;
    std::size_t lineNumber = 0;
    try {
        while (std::getline(input, line)) {
            ++lineNumber;
            // Accept a UTF-8 byte order mark on the first line.
            if (lineNumber == 1 && line.compare(0, 3, "\xEF\xBB\xBF") == 0) line.erase(0, 3);
            if (trim(line).empty()) continue;
            const auto fields = parseCsv(line);
            if (fields.size() < 2) throw std::runtime_error("A course number and title are required.");
            Course course;
            course.number = normalizeNumber(fields[0]);
            course.title = fields[1];
            if (!validNumber(course.number) || course.title.empty()) {
                throw std::runtime_error("Invalid course number or empty title.");
            }
            for (std::size_t index = 2; index < fields.size(); ++index) {
                const std::string prerequisite = normalizeNumber(fields[index]);
                if (!validNumber(prerequisite)) throw std::runtime_error("Invalid or empty prerequisite.");
                if (prerequisite == course.number) throw std::runtime_error("A course cannot require itself.");
                if (std::find(course.prerequisites.begin(), course.prerequisites.end(), prerequisite)
                    != course.prerequisites.end()) throw std::runtime_error("Duplicate prerequisite.");
                course.prerequisites.push_back(prerequisite);
                references.emplace_back(prerequisite, lineNumber);
            }
            if (!loaded.insert(std::move(course))) throw std::runtime_error("Duplicate course number.");
        }
        if (input.bad()) throw std::runtime_error("Could not finish reading the file.");
        if (loaded.size() == 0) throw std::runtime_error("The file contains no courses.");
        // Validate after loading so prerequisites may appear later in the file.
        for (const auto& reference : references) {
            if (!loaded.search(reference.first)) {
                lineNumber = reference.second;
                throw std::runtime_error("Prerequisite " + reference.first + " is missing from the file.");
            }
        }
    } catch (const std::runtime_error& error) {
        std::cout << "Error loading file at line " << lineNumber << ": " << error.what() << '\n';
        return false;
    }
    courses = std::move(loaded);
    std::cout << "Loaded " << courses.size() << " courses successfully.\n";
    return true;
}

void printCourse(const BinarySearchTree& courses, const std::string& number) {
    const Course* course = courses.search(number);
    if (!course) {
        std::cout << "Error: Course " << number << " was not found.\n";
        return;
    }
    std::cout << course->number << ", " << course->title << "\nPrerequisites:\n";
    if (course->prerequisites.empty()) std::cout << "  None\n";
    for (const auto& prerequisiteNumber : course->prerequisites) {
        const Course* prerequisite = courses.search(prerequisiteNumber);
        std::cout << "  " << prerequisite->number << ", " << prerequisite->title << '\n';
    }
}

int main() {
    BinarySearchTree courses;
    std::cout << "Welcome to the course planner.\n";
    std::string choice;
    while (true) {
        std::cout << "\n1. Load Data Structure.\n2. Print Course List.\n"
                     "3. Print Course.\n9. Exit\n\nWhat would you like to do? ";
        if (!std::getline(std::cin, choice)) break;
        choice = trim(choice);
        if (choice == "9") break;
        if (choice == "1") {
            std::string filename;
            std::cout << "Enter the course data filename: ";
            if (!std::getline(std::cin, filename)) break;
            loadCourses(trim(filename), courses);
        } else if (choice == "2" || choice == "3") {
            if (courses.size() == 0) {
                std::cout << "Error: Load the course data using option 1 first.\n";
                continue;
            }
            if (choice == "2") {
                std::cout << "Here is a sample schedule:\n";
                courses.printCourses();
            } else {
                std::string number;
                std::cout << "What course do you want to know about? ";
                if (!std::getline(std::cin, number)) break;
                number = normalizeNumber(number);
                if (!validNumber(number)) std::cout << "Error: Enter an alphanumeric course number.\n";
                else printCourse(courses, number);
            }
        } else {
            std::cout << "Error: " << choice << " is not a valid option. Choose 1, 2, 3, or 9.\n";
        }
    }
    std::cout << "Thank you for using the course planner!\n";
    return 0;
}
