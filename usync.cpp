#include <stdio.h>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <sstream>
#include <cpuid.h>
#include <cstring>
#include <algorithm>
#include <sstream>
#include <sys/stat.h>

namespace fs = std::filesystem;

struct SyncData {
  std::string machine;
  std::vector<std::string> paths;
};

std::vector<std::string> split(std::string& str, char sep) {
  std::stringstream ss(str);
  std::string segment;
  std::vector<std::string> seglist;

  while(std::getline(ss, segment, sep)) {
    seglist.push_back(segment);
  }

  return seglist;
}

// Source - https://stackoverflow.com/a/12774387
// Posted by PherricOxide, modified by community. See post 'Timeline' for change history
inline bool exists (const std::string& name) {
  struct stat buffer;
  return (stat (name.c_str(), &buffer) == 0);
}

int write(std::string& path, const std::vector<SyncData>& data) {
  std::ofstream conf_file(path, std::ios::trunc);

  if (!conf_file.is_open()) {
    printf("E: Unable to open '%s' for writing.\n", path.c_str());
    return 1;
  }

  for (int i = 0; i < data.size(); i++) {
    auto sec = data[i];
    conf_file << sec.machine << ":\n";

    for (int j = 0; j < sec.paths.size(); j++) {
      conf_file << "- " << sec.paths[j] << "\n";
    }
  }

  conf_file.close();

  return 0;
}

std::vector<SyncData> read(std::string& path) {
  std::ifstream conf_file(path);

  if (!conf_file.is_open()) {
    printf("E: Unable to open '%s' for reading.\n", path.c_str());
    return {};
  }

  std::vector<SyncData> data = {};
  SyncData tmp;
  std::string line;

  while (getline(conf_file, line)) {
    if (line.find_last_of(":") == line.size() - 1) {
      if (!tmp.machine.empty()) {
        data.push_back(tmp);
      }
      tmp.machine = line.substr(0, line.size() - 1);
      tmp.paths = {};
    }
    if (line.find_first_of("- ") == 0) {
      tmp.paths.push_back(line.substr(2, line.size() - 2));
    }
  }
  if (!tmp.machine.empty()) {
    data.push_back(tmp);
  }

  conf_file.close();

  return data;
}

void debug_display(std::vector<SyncData>& data) {
  for (int i = 0; i<data.size(); i++) {
    printf("%s:\n", data[i].machine.c_str());
    for (int j = 0; j<data[i].paths.size(); j++) {
      printf("- %s\n", data[i].paths[j].c_str());
    }
    printf("\n");
  }
}

int main (int argc, char *argv[]) {
  if (argc < 2) {
    printf("E: Missing argument 'to'.\n");
    return 1;
  }

  fs::path disk = argv[1];
  std::string conf_path = (disk/fs::path(".usync")).string();

  std::ofstream conf_file(conf_path, std::ios::app);

  if (!conf_file.is_open()) {
    printf("E: Unable to open '%s' for writing.\n", conf_path.c_str());
    return 1;
  }

  conf_file.close();


  // Get unique machine id
  std::string unique_machine_id;
  std::ifstream machine_id("/etc/machine-id");
  getline(machine_id, unique_machine_id);

  auto data = read(conf_path);

  if (argc > 2) {
    if (strcmp(argv[2], "add") == 0) {
      if (argc < 5) {
        printf("E: Missing arguments for add, must be 'source_path' 'dest_path'.\n");
        return 1;
      }
      bool found = false;

      std::string pathstr = argv[3] + std::string(":") + argv[4];

      for (int i = 0; i < data.size(); i++) {
        if (data[i].machine == unique_machine_id) {
          auto itr = std::find(data[i].paths.begin(), data[i].paths.end(), pathstr);
          if (itr == data[i].paths.end()) {
            data[i].paths.push_back(pathstr);
            found = true;
          }
          else {
            printf("I: This item is already being tracked.\n");
            return 0;
          }
        }
      }

      if (!found) {
        SyncData tmp;
        tmp.machine = unique_machine_id;
        tmp.paths = {pathstr};

        data.push_back(tmp);
      }

      write(conf_path, data);
    }

    if (strcmp(argv[2], "rm") == 0) {
      if (argc < 4) {
        printf("E: Missing argument 'source/dest_path' for rm.\n");
        return 1;
      }
      bool found = false;

      for (int i = 0; i < data.size(); i++) {
        if (data[i].machine == unique_machine_id) {
          int j = 0;
          for (int k = 0; k < data[i].paths.size(); k++) {
            auto r = split(data[i].paths[k], ':');
            if (strcmp(r[0].c_str(), argv[3]) == 0 || strcmp(r[1].c_str(), argv[3]) == 0) {
              found = true;
              break;
            }
            j++;
          }

          if (found) {
            data[i].paths.erase(data[i].paths.begin() + j);
          }
        }
      }

      if (!found) {
        printf("I: No entry exists for '%s'.\n", argv[3]);
        return 0;
      }

      write(conf_path, data);
    }

    if (strcmp(argv[2], "sync") == 0) {
      for (int i = 0; i < data.size(); i++) {
        if (data[i].machine == unique_machine_id) {
          for (int j = 0; j < data[i].paths.size(); j++) {
            auto r = split(data[i].paths[j], ':');
            std::string command = "mkdir -p $(dirname " + r[1] + ") && cp " + r[0] + " " + r[1];
            if (exists(r[0])) system(command.c_str());
          }
        }
      }
    }
  }

  return 0;
}
