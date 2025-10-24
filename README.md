# 🖥️ Interrupt Controller Simulation

## 📘 Overview
This project simulates the working of an **Interrupt Controller** to demonstrate how priorities and masking affect device handling in a computer system. It models multiple I/O devices generating interrupts, each with different priorities, and shows how an interrupt controller handles them efficiently.

---

## ⚙️ Features
- Simulates **three I/O devices**:
  - ⌨️ **Keyboard** – High Priority  
  - 🖱️ **Mouse** – Medium Priority  
  - 🖨️ **Printer** – Low Priority  
- Implements **priority-based interrupt handling**
- Supports **masking/unmasking** of devices at runtime
- Displays clear output messages during interrupt handling
- Optional: Logs each ISR execution with timestamps

---

## 🧩 Technologies Used
- **Language:** C++  
- **Concepts:** Multithreading, Synchronization, Interrupt Service Routine (ISR)  
- **Libraries:** `<thread>`, `<mutex>`, `<chrono>`, `<iostream>`

---

## 🧪 Example Output
