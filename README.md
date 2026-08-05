<p align="center">
  <img width="180" height="180" alt="G1_Logo_800" src="https://github.com/user-attachments/assets/8fb580ad-a908-46de-bb6f-9bf03002977b" />

</p>

<h1 align="center">InterviewSim</h1>

<p align="center">
<b>Practice. Perform. Improve.</b>
</p>

<p align="center">
AI-Based Virtual Interview Simulation and Skill Development System
</p>

<p align="center">

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-black)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-blue)
![Status](https://img.shields.io/badge/Status-Alpha%201.0-success)
![License](https://img.shields.io/badge/License-Academic-lightgrey)

</p>

<p align="center">

Developed by <b>Team G1</b>

Capstone Design Project

Department of Electrical and Computer Engineering

North South University

2026

</p>

---

# Table of Contents

- Overview
- Project Vision
- Key Features
- Why InterviewSim?
- System Workflow
- Interview Modes
- Technology Stack
- Development Team
- Supervisor
- Project Status
- Prototype Disclaimer
- Screenshots
- Demonstration
- Getting Started
- Acknowledgements
- License


---

# 📖 Overview

<p align="center">
<img width="588" height="327" alt="Splash Screen" src="https://github.com/user-attachments/assets/6ee3785d-5293-4ac5-abcd-f22e9323e4bb" />

</p>

InterviewSim is an AI-powered virtual interview simulation platform developed using **Unreal Engine 5**. The system provides a realistic interview environment through an AI-powered virtual interviewer, enabling users to practice technical interviews, improve communication skills, and receive intelligent performance feedback.

The application combines immersive virtual-human interaction, CV/Resume/Job Circular document analysis, AI-powered interview question generation, intelligent answer evaluation, structured reporting, and realistic interview simulation into a unified desktop application. Users can upload their own documents to generate personalized interview sessions tailored to their background and target job role.

InterviewSim was developed as the Capstone Design Project of **Team G1** at **North South University**.

---

# 🎯 Project Vision

InterviewSim was created with one primary objective:

> **Help candidates practice interviews, perform with confidence, and continuously improve through realistic AI-driven interview experiences.**

---

# ✨ Key Features

- Practice Interview Mode
- Real Interview Mode
- AI-Powered Virtual Interviewer
- CV / Resume / Job Circular Upload
- Document-Grounded Real Interview Generation
- AI-Powered Prompt Generation from Uploaded Documents
- Personalized Interview Question Generation
- Dynamic AI Question Generation
- Adaptive Question Selection
- Timed Interview Sessions
- Candidate Answer Evaluation
- AI-Generated Feedback
- Interview Performance Reports
- JSON Report Export
- TXT Transcript Export
- MetaHuman Integration
- Convai Speech Interaction
- Runtime Document Processing
- Automatic Question Difficulty Distribution
- Internet Connectivity Monitoring
- Graceful AI Fallback Mechanisms


# ✨ Why InterviewSim?

- Personalized Real Interviews from Uploaded Documents
- Resume-Aware AI Question Generation
- Job-Specific Interview Preparation
- Multi-Format Document Support (TXT, JSON, Text-based PDF)

---

# 🏗 System Workflow

```text
Candidate
     │
     ▼
   Upload
CV / Resume / Job Circular
     │
     ▼
Document Processing
     │
     ▼
Prompt Engine
     │
     ▼
AI Question Generation Engine
     │
     ▼
Interview Session Manager (FSM)
     │
     ▼
MetaHuman + Convai Interview
     │
     ▼
Evaluation Engine
     │
     ▼
Final Performance Report
(JSON / TXT)
```

---

# 🛠 Interview Modes

## 🎓 Practice Interview Mode

Practice Interview allows candidates to practice without uploading documents.

The virtual interviewer begins with an introduction question and dynamically generates interview questions based on the candidate's spoken or written introduction.

This mode is intended for general interview preparation and communication practice.

## 💼 Real Interview Mode

InterviewSim supports personalized interview sessions through document-guided interview generation.

Users can upload:

- CV
- Resume
- Job Circular

Supported formats:

- TXT
- JSON
- Text-based PDF

The uploaded document is analyzed by the Prompt Engine to identify relevant skills, experience, technologies, and job requirements. Based on this information, the Question Generation Engine creates a structured interview consisting of:

- Greeting
- 2 Warm-up Questions
- 5 Technical Questions
- 3 Behavioral Questions

This enables every Real Interview session to be tailored to the candidate's background and target position.
---
---

# 🛠 Technology Stack

<p align="center">

<img width="90" height="90" alt="unrealengine-com-logo" src="https://github.com/user-attachments/assets/744ac1ea-683b-4c62-a748-74d2203fd19f" />
&nbsp;&nbsp;&nbsp;

<img width="200" height="90" alt="metahuman" src="https://github.com/user-attachments/assets/264f43cb-f209-4b38-947a-7e1966bdbc99" />
&nbsp;&nbsp;&nbsp;

<img width="200" height="90" alt="convai2" src="https://github.com/user-attachments/assets/7703239b-8eb3-43dc-ab3e-478d07319329" />
&nbsp;&nbsp;&nbsp;

</p>

## Game Engine

- Unreal Engine 5.7 

- C++ 

## AI Services

- Google Gemini
- OpenRouter (Fallback)
- Convai

## Virtual Human

- MetaHuman

## Backend

- Python
- FastAPI
- Interview Evaluation Sidecar

## Supporting Technologies

- JSON
- HTTP REST API
- Unreal Motion Graphics (UMG)
- Windows Desktop Runtime

---

# 👥 Development Team

## Team G1

### QM Asif Tanjim

**Project Lead & Interview Engine Lead**

Responsible for:

- System Architecture
- Interview Session Manager (FSM)
- Interview Lifecycle
- Practice & Real Interview Workflow
- Convai Integration
- UI Integration
- Evaluation Integration
- Final System Integration
- Packaging & Release

---

### Nowren Mahjabin Khan

**Evaluation Engine & Server Lead**

Responsible for:

- Candidate Answer Evaluation
- AI Feedback Intelligence
- Answer Scoring
- Performance Reports
- JSON & TXT Export System

---

### Nawshin Kabir

**Prompt Engine Lead**

Responsible for:

- Prompt Generation Engine
- CV Processing
- Job Circular Processing
- Runtime Document Analysis
- Focused Interview Prompt Preparation

---

### Nazia Tasmin

**Question Generation Engine Lead**

Responsible for:

- AI Question Generation
- QuestionPool Management
- Question Selection Engine
- Difficulty Distribution
- Generation Callbacks
- Multi-AI Fallback Strategy

---

# 👨‍🏫 Project Supervisor

**Dr. Mohsin Sajjad**

Assistant Professor

Department of Electrical and Computer Engineering

North South University

Dhaka-1229, Bangladesh

---

# 📌 Project Status

## InterviewSim Alpha 1.0

**Development Status**

✅ Development Complete

**Current Phase**

Capstone Showcase & Demonstration

---

# ⚠ Prototype Disclaimer

InterviewSim Alpha 1.0 has been developed as an academic Capstone Design Project at **North South University**.

This software is a **prototype release** intended solely for:

- Educational purposes
- Research
- Demonstration
- Academic evaluation

It is **not intended for commercial use, production deployment, or real-world recruitment decisions.**

Future versions may extend the platform with additional features, improved AI capabilities, and production-ready deployment.

---

# 📷 Screenshots

### Splash Screen

*(Add image)*

### Main Menu

*(Add image)*

### Practice Interview

*(Add image)*

### Real Interview

*(Add image)*

### Results & Reports

*(Add image)*

---

# 🎥 Demonstration

Demo video will be added after the Capstone Showcase.

---

# 🚀 Getting Started

### Requirements

- Windows 10/11
- Internet Connection
- Microphone
- Speaker/Headphones

### Build

Built using:

- Unreal Engine 5.7
- Visual Studio 2026

---

# 🙏 Acknowledgements

Special thanks to:

- **Dr. Mohsin Sajjad** for his invaluable guidance and continuous support throughout the project.
- **Epic Games** for Unreal Engine and MetaHuman.
- **Convai** for conversational AI integration.
- **Google** for the Gemini API.
- **OpenRoute** for the OpenRoute API.
- The Department of Electrical and Computer Engineering, North South University.

---

# Future Work

- Dynamic profile updating during Practice Interview
- Additional interview domains
- Cloud deployment
- Recruiter dashboard
- Multi-language support
- Advanced analytics
- Production deployment

---

# 📄 License

This repository contains the source code of an academic Capstone Design Project.

Copyright© 2026 Team G1

All Rights Reserved.

This repository is intended for educational, research, and portfolio purposes.
