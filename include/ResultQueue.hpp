#pragma once

#include <queue>
#include <string>
#include <mutex>

struct ResultItem
{
    int client_fd;
    std::string responseString;
};


class ResultQueue{
public:
    void push(ResultItem result){
        std::lock_guard<std::mutex> lock(mtx);
        resultq.push(std::move(result));
    }

    void drainInto(std::vector<ResultItem>& resultList){
        std::lock_guard<std::mutex> lock(mtx);
        while (!resultq.empty())
        {
            resultList.push_back(std::move(resultq.front()));
            resultq.pop();
        }
    }

private:
    std::queue<ResultItem> resultq;
    std::mutex mtx;
};