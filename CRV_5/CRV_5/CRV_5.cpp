#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <memory>

const int EXPLOSION_LIMIT = 10000;
const int INITIAL_NUGGETS = 3000;
const int SIMULATION_TIME = 5; // 5 секунд = 5 дней

class CustomMutex {
private:
    std::atomic<bool> locked;

public:
    CustomMutex() : locked(false) {}

    void lock() {
        bool expected = false;
        // Пытаемся захватить мьютекс с помощью compare_exchange
        while (!locked.compare_exchange_weak(expected, true,
            std::memory_order_acquire,
            std::memory_order_relaxed)) {
            expected = false; // Сбрасываем expected после неудачи
            // Даем возможность другим потокам поработать
            std::this_thread::yield();
        }
    }

    void unlock() {
        locked.store(false, std::memory_order_release);
    }
};

class FatManSimulation {
private:
    CustomMutex mtx;
    std::vector<int> dishes;
    std::vector<int> eaten;
    std::atomic<bool> simulation_running;
    std::atomic<bool> cook_can_work;
    std::atomic<bool> fatmen_can_eat;
    std::atomic<int> fatmen_done;

    int gluttony;
    int efficiency;
    std::string outcome;

public:
    FatManSimulation(int gluttony_val, int efficiency_val)
        : gluttony(gluttony_val), efficiency(efficiency_val) {
        reset();
    }

    void reset() {
        dishes = { INITIAL_NUGGETS, INITIAL_NUGGETS, INITIAL_NUGGETS };
        eaten = { 0, 0, 0 };
        simulation_running = true;
        cook_can_work = true;
        fatmen_can_eat = false;
        fatmen_done = 0;
        outcome = "Не определен";
    }

    void cook() {
        while (simulation_running) {
            // Ждем разрешения работать
            while (!cook_can_work && simulation_running) {
                std::this_thread::yield();
            }

            if (!simulation_running) break;

            mtx.lock();

            // Добавляем наггетсы на все тарелки
            for (int i = 0; i < 3; ++i) {
                dishes[i] += efficiency;
            }

            // Готовим следующий шаг
            cook_can_work = false;
            fatmen_can_eat = true;
            fatmen_done = 0;

            mtx.unlock();

            // Пауза между приготовлениями (1 день)
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void fatman(int id) {
        while (simulation_running) {
            // Ждем разрешения есть
            while (!fatmen_can_eat && simulation_running) {
                std::this_thread::yield();
            }

            if (!simulation_running) break;

            mtx.lock();

            // Проверяем условия
            bool any_empty = (dishes[0] <= 0) || (dishes[1] <= 0) || (dishes[2] <= 0);
            bool all_burst = (eaten[0] >= EXPLOSION_LIMIT) &&
                (eaten[1] >= EXPLOSION_LIMIT) &&
                (eaten[2] >= EXPLOSION_LIMIT);

            if (any_empty) {
                outcome = "Кука уволили! (Тарелка опустела)";
                simulation_running = false;
                mtx.unlock();
                break;
            }

            if (all_burst) {
                outcome = "Кук не получил зарплату! (Все толстяки лопнули)";
                simulation_running = false;
                mtx.unlock();
                break;
            }

            // Едим, если можем
            if (dishes[id] > 0 && eaten[id] < EXPLOSION_LIMIT) {
                int to_eat = std::min(gluttony, dishes[id]);
                to_eat = std::min(to_eat, EXPLOSION_LIMIT - eaten[id]);

                dishes[id] -= to_eat;
                eaten[id] += to_eat;
            }

            fatmen_done++;

            // Если все три толстяка поели, передаем ход повару
            if (fatmen_done >= 3) {
                fatmen_can_eat = false;
                cook_can_work = true;
            }

            mtx.unlock();

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

    std::string run() {
        // Запускаем потоки
        std::thread cook_thread(&FatManSimulation::cook, this);
        std::vector<std::thread> fatmen;
        for (int i = 0; i < 3; ++i) {
            fatmen.emplace_back(&FatManSimulation::fatman, this, i);
        }

        // Ждем 5 секунд
        auto start = std::chrono::steady_clock::now();
        while (simulation_running) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start);
            if (elapsed.count() >= SIMULATION_TIME) {
                if (simulation_running) {
                    outcome = "Кук уволился сам! (Прошло 5 дней)";
                }
                simulation_running = false;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Останавливаем потоки
        cook_can_work = true;
        fatmen_can_eat = true;

        if (cook_thread.joinable()) cook_thread.join();
        for (auto& t : fatmen) {
            if (t.joinable()) t.join();
        }

        return outcome;
    }

    void print_results() const {
        std::cout << "Финальное состояние:" << std::endl;
        for (int i = 0; i < 3; ++i) {
            std::cout << "  Толстяк #" << i + 1 << ": съел " << eaten[i]
                << ", осталось на тарелке " << dishes[i] << std::endl;
        }
    }

    std::string get_outcome() const { return outcome; }
};

int main() {
    setlocale(LC_ALL, "Russian");

    std::cout << "ЛАБОРАТОРНАЯ РАБОТА №5" << std::endl;
    std::cout << "========================" << std::endl;
    std::cout << "Используется кастомный мьютекс на основе атомарных переменных" << std::endl;
    std::cout << "Методы: lock() и unlock() с compare_exchange_weak" << std::endl;
    std::cout << "========================" << std::endl;

    
    struct Scenario {
        int gluttony;
        int efficiency;
        std::string name;
        std::string expected;
    };

    std::vector<Scenario> scenarios = {
        {100, 5,   "СЦЕНАРИЙ 1: Кука уволили",        "Кука уволили! (Тарелка опустела)"},
        {100, 1000, "СЦЕНАРИЙ 2: Толстяки лопнули",   "Кук не получил зарплату! (Все толстяки лопнули)"},
        {10,  11,   "СЦЕНАРИЙ 3: Кук уволился сам",   "Кук уволился сам! (Прошло 5 дней)"}
    };

    int scenario_num = 1;
    for (const auto& scenario : scenarios) {
        std::cout << "\n=== " << scenario.name << " ===" << std::endl;
        std::cout << "Коэффициенты: gluttony=" << scenario.gluttony
            << ", efficiency=" << scenario.efficiency << std::endl;
        std::cout << "Ожидаемый исход: " << scenario.expected << std::endl;

        // Создаем и запускаем симуляцию с кастомным мьютексом
        FatManSimulation sim(scenario.gluttony, scenario.efficiency);
        std::string result = sim.run();

        std::cout << "Фактический исход: " << result << std::endl;
        sim.print_results();

        // Проверяем соответствие ожидаемому результату
        if (result == scenario.expected) {
            std::cout << "✓ Сценарий " << scenario_num << " выполнен успешно!" << std::endl;
        }
        else {
            std::cout << "✗ Сценарий " << scenario_num << " не соответствует ожиданиям" << std::endl;
        }

        scenario_num++;
    }

    std::cout << "\n================================" << std::endl;
    std::cout << "Все сценарии завершены!" << std::endl;
    std::cout << "Кастомный мьютекс работает корректно." << std::endl;

    return 0;
}