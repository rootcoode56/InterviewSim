<p align="center">
  <img width="180" height="180" alt="G1_Logo_800" src="https://github.com/user-attachments/assets/8fb580ad-a908-46de-bb6f-9bf03002977b" />
</p>

<h1 align="center">InterviewSim</h1>

<p align="center">
  <b>Practice. Perform. Improve.</b>
</p>

<p align="center">
  <b>AI-Based Virtual Interview Simulation and Skill Development System</b>
</p>

<p align="center">

![Unreal Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-black)
![C++](https://img.shields.io/badge/C%2B%2B-17-blue)
![Platform](https://img.shields.io/badge/Platform-Windows-blue)
![Status](https://img.shields.io/badge/Status-Alpha%201.0-success)
![License](https://img.shields.io/badge/License-Academic-lightgrey)

</p>

<p align="center">

Developed by <b>Team G1</b><br>
Capstone Design Project<br>
Department of Electrical and Computer Engineering<br>
North South University<br>
2026

</p>

---

# 📑 Table of Contents

- [Overview](#-overview)
- [Project Vision](#-project-vision)
- [Key Features](#-key-features)
- [Why InterviewSim?](#-why-interviewsim)
- [System Workflow](#-system-workflow)
- [Interview Modes](#-interview-modes)
- [Interview Structure](#-interview-structure)
- [System Architecture](#-system-architecture)
- [Technology Stack](#-technology-stack)
- [Development Team](#-development-team)
- [Detailed Contribution Breakdown](#-detailed-contribution-breakdown)
- [Collaborative Integration Notes](#-collaborative-integration-notes)
- [Project Supervisor](#-project-supervisor)
- [Alpha Milestone](#-alpha-milestone)
- [Project Status](#-project-status)
- [Prototype Disclaimer](#-prototype-disclaimer)
- [Security Notice](#-security-notice)
- [Screenshots](#-screenshots)
- [Demonstration](#-demonstration)
- [Getting Started](#-getting-started)
- [Future Work](#-future-work)
- [Acknowledgements](#-acknowledgements)
- [License](#-license)

---

# 📖 Overview

<p align="center">
  <img width="588" height="327" alt="InterviewSim Splash Screen" src="https://github.com/user-attachments/assets/6ee3785d-5293-4ac5-abcd-f22e9323e4bb" />
</p>

**InterviewSim** is an AI-powered virtual interview simulation and skill-development platform built with **Unreal Engine 5.7**.

The system combines an immersive MetaHuman interviewer, real-time speech interaction, dynamic interview question generation, CV / Resume / Job Circular analysis, candidate-answer evaluation, structured feedback, timed interview sessions, and final performance reporting into a unified Windows desktop application.

InterviewSim supports two major interview experiences:

- **Practice Interview** — personalized from the candidate's own introduction and background.
- **Real Interview** — grounded in uploaded CV / Resume / Job Circular documents and tailored to the target role.

The system was developed as the Capstone Design Project of **Team G1** at the **Department of Electrical and Computer Engineering, North South University**.

---

# 🎯 Project Vision

InterviewSim was created around one central objective:

> **Practice. Perform. Improve.**

The goal is to give candidates a more realistic and interactive environment for interview preparation by combining:

- Conversational AI
- Virtual human interaction
- Document understanding
- Adaptive question generation
- Structured interview logic
- Automated answer evaluation
- Actionable performance feedback

Rather than presenting only static question lists, InterviewSim attempts to simulate a complete interview lifecycle from introduction to closing and final evaluation.

---

# ✨ Key Features

- Practice Interview Mode
- Real Interview Mode
- MetaHuman Virtual Interviewer
- Convai Conversational AI Integration
- Speech-to-Text Candidate Input
- Typed Candidate Input
- AI-Based Question Generation
- Multi-AI Question Generation Fallback
- Personalized Practice Questions
- CV / Resume / Job Circular Upload
- TXT / JSON / Text-Based PDF Processing
- Runtime Document Analysis
- Focused Interview Prompt Generation
- Document-Grounded Real Interviews
- Question Pool Management
- Stage-Based Question Selection
- Difficulty-Aware Question Selection
- Structured Interview Finite State Machine
- Timed 5 / 10 / 15 Minute Interviews
- Pause / Resume Support
- Candidate Interaction Locking
- AI-Based Answer Evaluation
- Strength & Weakness Identification
- Grammar / Repetition Tracking
- Detailed Performance Feedback
- JSON Evaluation Report Export
- TXT Transcript Export
- Partial Interview Finalization
- Final Performance Score Persistence
- Internet Connectivity Monitoring
- Graceful AI Fallback Handling
- Interview Session Restart / Cleanup
- Windows Shipping Build Support

---

# 💡 Why InterviewSim?

Traditional interview preparation tools often provide:

- Static question lists
- Text-only interaction
- Generic practice sessions
- Little or no personalized feedback

InterviewSim was designed to provide a more complete interview experience.

## InterviewSim can:

- Personalize questions from a candidate's introduction.
- Generate role-specific questions from CV and job information.
- Ask technical, behavioral, and situational questions.
- Conduct the session through an AI-powered MetaHuman interviewer.
- Accept both spoken and written answers.
- Evaluate candidate responses.
- Produce structured performance feedback.
- Export interview results for later review.
- Simulate interview timing and stage progression.

---

# 🏗 System Workflow

```text
                         Candidate
                            │
                            ▼
                  Select Interview Mode
                            │
             ┌──────────────┴──────────────┐
             │                             │
             ▼                             ▼
      Practice Interview              Real Interview
             │                             │
     Candidate Introduction      Upload CV / Resume /
             │                    Job Circular
             │                             │
             ▼                             ▼
  Candidate Background Context      Document Processing
             │                             │
             │                             ▼
             │                        Prompt Engine
             │                             │
             └──────────────┬──────────────┘
                            │
                            ▼
                 Question Generation Engine
                            │
                            ▼
                   Question Pool Manager
                            │
                            ▼
                Interview Session Manager
                         (FSM)
                            │
                            ▼
                 MetaHuman + Convai
                  Virtual Interviewer
                            │
                            ▼
                    Candidate Answer
                            │
                            ▼
                    Evaluation Engine
                            │
                            ▼
                 Final Interview Report
                     JSON / TXT Export
```

---

# 🛠 Interview Modes

## 🎓 Practice Interview Mode

Practice Interview allows the candidate to begin without uploading documents.

The interviewer starts with a Greeting question asking the candidate to introduce themselves. The candidate's introduction is then used as the context for personalized question generation.

The Practice flow includes:

```text
Greeting
   ↓
Personalized Question Preparation
   ↓
Warm-up
   ↓
Technical
   ↓
Behavioral
   ↓
Closing
   ↓
Final Evaluation
```

The system can use:

1. **Gemini** as the primary question-generation provider.
2. **OpenRouter** as an AI fallback.
3. **Local Fallback Questions** if external AI generation is unavailable.

Practice LocalFallback generation also uses candidate background topics from the Greeting answer so that Warm-up and Technical questions remain relevant to the candidate where possible.

---

## 💼 Real Interview Mode

Real Interview creates a document-grounded interview based on candidate and job information.

Users may upload:

- CV
- Resume
- Job Circular
- CV + Job Circular

Supported formats:

- TXT
- JSON
- Text-based PDF

The document-processing pipeline extracts and sanitizes relevant text, validates the supplied inputs, and creates a focused interview context.

The system identifies information such as:

- Technical skills
- Work experience
- Education
- Tools and technologies
- Projects
- Responsibilities
- Job requirements
- Required qualifications
- Target-role expectations

The focused context is then passed to the Question Generation Engine, which creates a structured set of questions for the interview.

---

# 🔢 Interview Structure

InterviewSim Alpha 1.0 uses the following final interview structure:

```text
Greeting     : 1 Question
Warm-up      : 2 Questions
Technical    : 5 Questions
Behavioral     : 3 Questions
--------------------------------
Total         : 11 Questions
```

The Question Generation Engine generates **10 questions excluding the Greeting**.

## Difficulty Distribution

```text
Warm-up
└── 2 Easy

Technical
├── 2 Easy
├── 2 Medium
└── 1 Hard

Behavioral
└── 3 Medium
```

The Interview Session Manager controls when each question is selected and asked.

An interview may also end earlier if the selected 5 / 10 / 15 minute timer reaches zero before all 11 answers are submitted. In that case, InterviewSim performs a valid **partial interview finalization** using the answers already submitted.

---

# 🧠 System Architecture

InterviewSim is composed of multiple integrated subsystems.

```text
┌─────────────────────────────────────────────┐
│               USER INTERFACE                │
│        UMG Widgets / Interview UI           │
└─────────────────────┬───────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────┐
│           INTERVIEW GAMEPLAY LAYER          │
│                                             │
│   InterviewSimGameMode                      │
│   InterviewSessionManager                   │
│   Timer / Input / Session Lifecycle         │
│   Interview State Machine                   │
└──────────┬─────────────┬──────────────┬─────┘
           │             │              │
           ▼             ▼              ▼
┌────────────────┐ ┌──────────────┐ ┌─────────────────┐
│ Prompt Engine  │ │ Question     │ │ Evaluation      │
│                │ │ Generation   │ │ & Feedback      │
│ CV / Job       │ │ Engine       │ │ Intelligence    │
│ Processing     │ │              │ │                 │
└───────┬────────┘ └──────┬───────┘ └────────┬────────┘
        │                 │                   │
        └─────────────────┼───────────────────┘
                          │
                          ▼
              ┌──────────────────────┐
              │ Convai + MetaHuman   │
              │ Virtual Interviewer  │
              └──────────────────────┘
```

---

# 🛠 Technology Stack

<p align="center">

<img width="90" height="90" alt="Unreal Engine Logo" src="https://github.com/user-attachments/assets/744ac1ea-683b-4c62-a748-74d2203fd19f" />
&nbsp;&nbsp;&nbsp;

<img width="200" height="90" alt="MetaHuman Logo" src="https://github.com/user-attachments/assets/264f43cb-f209-4b38-947a-7e1966bdbc99" />
&nbsp;&nbsp;&nbsp;

<img width="200" height="90" alt="Convai Logo" src="https://github.com/user-attachments/assets/7703239b-8eb3-43dc-ab3e-478d07319329" />
&nbsp;&nbsp;&nbsp;

</p>

## Game Engine & Application

- Unreal Engine 5.7
- C++
- Blueprint Visual Scripting
- Unreal Motion Graphics (UMG)

## Virtual Human & Conversation

- MetaHuman
- Convai
- Convai Speech-to-Text
- Convai Text-to-Speech / Character Speech

## AI Services

- Google Gemini
- OpenRouter
- Local Fallback Logic

## Backend / Evaluation Support

- Python
- FastAPI
- Optional Interview Evaluation Sidecar

## Document Processing

- TXT
- JSON
- Text-Based PDF
- Runtime Text Extraction
- Prompt Sanitization

## Communication & Data

- HTTP REST API
- JSON
- Unreal HTTP Module
- JSON / TXT Local Report Export

## Development Platform

- Windows
- Visual Studio
- Unreal Engine Editor

---

# 👥 Development Team

## Team G1

InterviewSim was developed collaboratively by:

- **QM Asif Tanjim**
- **Nazia Tasmin**
- **Nawshin Kabir**
- **Nowren Mahjabin Khan**

The team followed a modular development structure. Each member primarily developed a major subsystem, and the modules were later integrated into the final InterviewSim Alpha application.

---

# 🧩 Detailed Contribution Breakdown

## 👨‍💻 QM Asif Tanjim

### Project Lead / Core Interview Architecture & Integration Lead

Primary responsibility: **the runtime interview system that connects all modules into one working application.**

### Core Interview Architecture

- Designed and implemented the central interview runtime architecture.
- Developed the **Interview Session Manager**.
- Implemented the authoritative interview **Finite State Machine (FSM)**:

```text
Greeting
→ Warm-up
→ Technical
→ Behavioral
→ Closing
→ Finished
```

- Managed stage progression, question counters, difficulty routing, and interview completion rules.
- Implemented support for the final 11-question interview structure.
- Implemented correct handling when the timer ends before all questions are completed.

### Interview Lifecycle & Timer

- Implemented configurable **5 / 10 / 15 minute interview durations**.
- Implemented timer start, pause, resume, reset, and expiry behavior.
- Implemented safe timer-expiry transition into Closing.
- Resolved race conditions involving timer expiry while Auraa was still speaking.
- Ensured Closing/outro completes before final results are displayed.
- Implemented partial interview finalization when a user exits an incomplete interview.

### Practice & Real Interview Orchestration

- Integrated Practice and Real Interview workflows into a shared runtime system.
- Connected Practice Greeting answers to personalized question generation.
- Coordinated waiting/loading states while Practice questions are being prepared.
- Integrated Real Interview prompt input with question generation.
- Managed interview-ready events and loading-screen transitions.
- Implemented runtime behavior for new interviews, re-entry, reset, and mode switching.

### Convai & MetaHuman Integration

- Integrated Convai runtime interaction with the MetaHuman interviewer **Auraa**.
- Managed Convai session start/stop/restart behavior.
- Integrated speech delivery with the interview FSM.
- Integrated candidate PTT / STT interaction into the answer-submission pipeline.
- Added speaking / turn-pending interaction locks to prevent candidate input during Auraa's turn.
- Implemented safe pause blocking while Auraa is speaking or preparing to speak.
- Resolved session-restart and stale-session issues across:
  - Practice → New Interview
  - Practice → Real Interview
  - Exit → Main Menu → New Interview
- Implemented/integrated neutral interviewer acknowledgements.
- Integrated derailment warning behavior without disrupting the interview FSM.

### UI / Widget / Gameplay Integration

- Integrated the Unreal UMG interface with the C++ interview runtime.
- Connected Main Menu, Interview Selection, Practice / Real setup, loading, pause, confirmation, interview, and score interfaces.
- Implemented UI bridges through the GameMode.
- Integrated interview status messaging and candidate input state.
- Integrated loading-screen readiness behavior.
- Integrated final score/performance display.
- Implemented/stabilized:
  - Continue Interview
  - New Interview
  - Exit to Main Menu
  - Main Menu Exit
  - Performance screen navigation
  - Meet Team screen flow

### Cross-Module Integration

Integrated the team's independently developed systems into the final application:

- Nawshin's Prompt Engine
- Nazia's Question Generation / Question Pool system
- Nowren's Evaluation & Feedback system
- Convai / MetaHuman conversation flow

### Evaluation Integration

- Connected candidate-answer submission to the lightweight C++ evaluation pipeline.
- Integrated detailed asynchronous evaluation updates into the runtime workflow.
- Ensured evaluation never blocks interview progression.
- Integrated final report handling with gameplay and UI.
- Supported correct answer-input typing:
  - Written text
  - Speech / STT

### Reliability, Debugging & Stabilization

Led extensive final integration debugging and regression testing, including fixes for:

- Transcript clearing
- STT answer fragmentation
- Duplicate questions
- Duplicate interviewer lines
- Candidate counter desynchronization
- Interview UI disappearance
- Early result-screen behavior
- Auraa silence/session issues
- Real Interview generic-question failures
- Practice stale UI
- Input/pause timing
- Timer / Closing race conditions
- Session-restart problems
- New Interview cleanup
- Exit-to-MainMenu cleanup
- Loading-screen readiness
- Performance-score persistence
- Standalone / Shipping validation
- Final Windows release readiness

### Question-Generation Reliability Integration

Nazia remains the owner of the Question Generation module. During final integration, Asif also implemented reliability/configuration fixes required by the integrated application, including:

- Moving the OpenRouter credential from hardcoded source to external configuration.
- Updating the OpenRouter fallback model when the previous model required paid credits.
- Adding OpenRouter reasoning configuration required by the selected free model.
- Extending response parsing to support multiple compatible JSON field-name formats.
- Validating the final Gemini → OpenRouter → LocalFallback runtime chain.

These were **integration/reliability modifications to Nazia's module**, not a transfer of module ownership.

### Release Engineering

- Led final Alpha stabilization.
- Performed targeted regression testing.
- Built and validated Windows Shipping releases.
- Prepared multiple Convai-account showcase builds for redundancy.
- Diagnosed Shipping-only configuration issues.
- Completed final packaged-application acceptance testing.

---

## 📊 Nowren Mahjabin Khan

### Evaluation & Feedback Intelligence Lead

Primary responsibility: **evaluating candidate answers and transforming interview responses into structured feedback and performance reports.**

### Candidate Answer Evaluation

Developed the evaluation logic used to assess candidate responses.

Evaluation fields include:

- Score
- Correctness
- Clarity
- Relevance
- Confidence
- Grammar issues
- Repetition
- Feedback
- Strengths
- Weaknesses

### Lightweight Runtime Evaluation

- Implemented/contributed to the lightweight evaluation path used directly inside the Unreal interview runtime.
- Designed the evaluation flow so that the candidate receives structured assessment data without blocking the core interview lifecycle.
- Supported preservation of evaluation results for each submitted answer.

### Detailed Evaluation / Feedback Intelligence

- Developed the detailed evaluation and feedback workflow associated with the Evaluation module.
- Supported AI-enhanced analysis for richer feedback when the detailed evaluation service is available.
- Supported fallback behavior so lightweight evaluation remains available when external detailed evaluation is unavailable.

### Answer-Type Awareness

- Supported distinction between candidate answer input methods.
- Final integrated runtime stores answer type such as:

```text
written_text
speech_stt
```

This allows exported reports to correctly represent whether the candidate typed or spoke an answer.

### Repetition & Communication Analysis

- Supported repeated-answer detection.
- Tracked grammar-related issues.
- Contributed to clarity/confidence / relevance-oriented feedback.
- Produced structured strengths and weaknesses for each interview.

### Final Performance Reporting

Developed the evaluation data used for final interview reporting, including:

- Overall performance score
- Strengths
- Weaknesses
- Repetition status
- Grammar information
- Per-answer evaluation
- Final interview summary information

### Report Export

Implemented/owned the evaluation/report data used for:

- JSON Report Export
- TXT Transcript Export

Reports preserve interview-answer records, evaluation information, stages, answer types, and final performance data.

---

## 🤖 Nazia Tasmin

### Multi-AI Question Generation & Selection Engine Lead

Primary responsibility: **generating, validating, storing, and selecting interview questions for Practice and Real Interview modes.**

### AI Question Generation

- Developed the AI-based Question Generation Engine.
- Integrated **Google Gemini** as the primary generation provider.
- Integrated **OpenRouter** as the secondary AI fallback.
- Implemented **LocalFallback** as the final fallback when external AI services are unavailable.

Final generation chain:

```text
Gemini
   ↓ failure
OpenRouter
   ↓ failure
LocalFallback
```

### Practice Interview Question Generation

- Generated personalized interview questions from the candidate's Greeting / introduction answer.
- Produced the 10 generated questions required after the initial Greeting.
- Supported candidate-background-aware Warm-up and Technical questions.
- Supported generic Behavioral questions when appropriate.
- Broadcast question-generation completion through runtime callbacks.

### Real Interview Question Generation

- Generated Real Interview questions from the focused prompt created by the Prompt Engine.
- Used candidate and target-job context for document-grounded question generation.
- Enforced the required Real Interview distribution:

```text
2 Warm-up Easy
2 Technical Easy
2 Technical Medium
1 Technical Hard
3 Behavioral Medium
```

### Validation & Regeneration

- Implemented validation of generated question sets.
- Rejected invalid distributions.
- Supported strict regeneration when the first generated set did not satisfy the required structure.
- Ensured invalid or malformed AI output did not silently enter the active Question Pool.

### Question Pool & Selection

- Developed / managed the **Question Pool Manager**.
- Stored generated questions for runtime use.
- Implemented stage- and difficulty-aware question retrieval.
- Supplied the Interview Session Manager with the best available question for the required stage and difficulty.

### AI Response Handling

- Implemented structured JSON parsing for generated questions.
- Managed AI response callbacks.
- Prevented duplicate question-return completion behavior.
- Supported fallback recovery when AI providers fail or return unusable output.

---

## 📄 Nawshin Kabir

### Prompt Engine & Runtime Document Processing Lead

Primary responsibility: **converting candidate/job documents into safe, focused interview context for Real Interview mode.**

### Prompt Engine

- Developed the **Prompt Engine subsystem** used by Real Interview.
- Designed the pipeline that converts user-provided documents into a focused interview-generation prompt.
- Exposed the Prompt Engine to the integrated runtime through Unreal subsystem / Blueprint-accessible interfaces.

### Document Input Support

Implemented runtime processing for:

- TXT
- JSON
- Text-based PDF

### CV / Resume Processing

- Extracted candidate information from uploaded CV / Resume documents.
- Processed candidate skills, experience, education, projects, and technologies.
- Prepared candidate context for question generation.

### Job Circular Processing

- Extracted role requirements and relevant information from uploaded Job Circulars.
- Processed expected technologies, qualifications, responsibilities, and job context.
- Prepared job-specific context for interview generation.

### Combined CV + Job Mode

Supported three Real Interview input modes:

```text
CV Only
Job Circular Only
CV + Job Circular
```

The combined mode builds interview context using both candidate background and target-job requirements.

### Validation & Sanitization

- Implemented document-type and input validation.
- Rejected unsupported/invalid document combinations.
- Rejected scanned or unusable PDF input where text extraction is not possible.
- Sanitized personally identifiable/unnecessary information before prompt generation.
- Produced focused interview context rather than passing raw document content directly to the Question Generation Engine.

### Upload / UI Integration Support

- Developed the core document-upload workflow used by the Real Interview setup.
- Supported file-selection and submission behavior.
- Connected successful document processing to the final focused prompt used by the integrated system.

---

# 🤝 Collaborative Integration Notes

InterviewSim is an integrated project. Some final-runtime changes affected modules originally developed by another team member.

To preserve contribution ownership accurately:

### Question Generation

**Primary owner:** Nazia Tasmin

Asif performed final integration/reliability modifications required for the packaged application, including OpenRouter credential loading, fallback-model compatibility, parser compatibility, and runtime validation.

### Prompt Engine

**Primary owner:** Nawshin Kabir

Asif integrated the Prompt Engine into Real Interview mode and connected its focused prompt output to the shared interview startup flow.

### Evaluation & Feedback

**Primary owner:** Nowren Mahjabin Khan

Asif integrated evaluation submission, asynchronous detailed-evaluation updates, finalization, performance display, and runtime lifecycle behavior into the central interview system.

### Conversational AI

Convai / MetaHuman runtime behavior became a shared integration area during development.

The final application uses Convai for Auraa's speech / STT interaction, while Asif handled the final interview-session lifecycle, interaction locks, speech-completion routing, restart behavior, and integration with the Interview Session Manager.

### Final Application

The Windows Alpha application is the **combined Team G1 product**.

No single module represents the complete application independently.

---

# 🗂 Primary Module Ownership

| Area | Primary Contributor |
|---|---|
| Core System Architecture | QM Asif Tanjim |
| Interview Session Manager / FSM | QM Asif Tanjim |
| Timer & Interview Lifecycle | QM Asif Tanjim |
| Gameplay / UI Runtime Integration | QM Asif Tanjim |
| Convai / MetaHuman Runtime Integration | QM Asif Tanjim |
| Cross-Module Integration | QM Asif Tanjim |
| Final Stabilization / Packaging / Release | QM Asif Tanjim |
| AI Question Generation | Nazia Tasmin |
| Question Pool Management | Nazia Tasmin |
| Question Selection & Difficulty Distribution | Nazia Tasmin |
| Multi-AI Question Fallback | Nazia Tasmin |
| Prompt Generation | Nawshin Kabir |
| CV / Resume Processing | Nawshin Kabir |
| Job Circular Processing | Nawshin Kabir |
| Runtime Document Analysis | Nawshin Kabir |
| Candidate Answer Evaluation | Nowren Mahjabin Khan |
| Feedback Intelligence | Nowren Mahjabin Khan |
| Strength / Weakness Analysis | Nowren Mahjabin Khan |
| Evaluation Report Data / Export | Nowren Mahjabin Khan |

---

# 👨‍🏫 Project Supervisor

**Dr. Mohsin Sajjad**

Assistant Professor  
Department of Electrical and Computer Engineering  
North South University  
Dhaka-1229, Bangladesh

---

# 🏆 Alpha Milestone

## InterviewSim Alpha 1.0

InterviewSim Alpha 1.0 was completed in **August 2026**.

The system reached a complete and demonstrable state with:

- Practice Interview
- Real Interview
- Dynamic question generation
- Multi-AI fallback
- CV / Job document processing
- Prompt generation
- MetaHuman interviewer
- Convai speech interaction
- Candidate answer evaluation
- Performance feedback
- Report export
- Session lifecycle management
- Timed interviews
- Error/fallback handling
- Windows Shipping builds

The completed project was selected for presentation at:

## NSU ACM Innovation Challenge — Season 20

InterviewSim was demonstrated to industry judges as one of the selected projects.

During the Innovation Challenge, judges discussed future product directions including:

- Business model development
- Subscription-based services
- Candidate-data security
- Privacy protection
- Production-ready deployment
- Real-world robustness
- More realistic and varied interview scenarios

The Innovation Challenge presentation marked the official completion of the **InterviewSim Alpha journey**.

---

# 📌 Project Status

## InterviewSim Alpha 1.0

| Area | Status |
|---|---|
| Core Development | ✅ Complete |
| Practice Interview | ✅ Complete |
| Real Interview | ✅ Complete |
| Prompt Engine | ✅ Complete |
| Question Generation | ✅ Complete |
| Evaluation & Feedback | ✅ Complete |
| Convai / MetaHuman Integration | ✅ Complete |
| Cross-Module Integration | ✅ Complete |
| Bug Fixing / Stabilization | ✅ Complete |
| Regression Testing | ✅ Complete |
| Windows Shipping Build | ✅ Complete |
| Capstone Demonstration | ✅ Complete |
| NSU ACM Innovation Challenge | ✅ Presented |
| Alpha Phase | ✅ Complete |

---

# ⚠ Prototype Disclaimer

InterviewSim Alpha 1.0 was developed as an academic Capstone Design Project at **North South University**.

This software is a prototype intended for:

- Education
- Research
- Academic evaluation
- Demonstration
- Portfolio presentation

InterviewSim Alpha 1.0 is **not intended for commercial deployment, production recruitment, automated employment decisions, or other high-stakes decision-making**.

The current prototype relies on external AI and conversational services and would require additional security, privacy, infrastructure, compliance, and reliability work before production use.

---

# 📷 Screenshots

## Splash Screen

<p align="center">
  <img width="588" height="327" alt="InterviewSim Splash Screen" src="https://github.com/user-attachments/assets/6ee3785d-5293-4ac5-abcd-f22e9323e4bb" />
</p>

---

## Main Menu

<img width="1918" height="1078" alt="MainMenu" src="https://github.com/user-attachments/assets/a28238b8-d805-4530-8d28-7162cc23a9ea" />


---

## Interview Setup

<img width="1918" height="1078" alt="IntSetup" src="https://github.com/user-attachments/assets/85fbdfa9-3f9f-4bb6-9197-c4fa2bff2343" />


---

# 🎥 Demonstration

A demonstration of InterviewSim is available here:
([https://www.youtube.com/watch?v=iUhniLyqRsw](https://youtu.be/bbIqlUCHXxI))

The demonstration presents the major features of the Alpha workflow and application.

---

# 🚀 Getting Started

## Running the Alpha Application

### Requirements

- Windows 10 / 11
- Internet Connection
- Microphone
- Speaker / Headphones

The packaged Windows Alpha application may be distributed separately through an authorized project release.

---

## Development Environment

InterviewSim was developed with:

```text
Unreal Engine 5.7
C++
Blueprint Visual Scripting
Visual Studio
UMG
MetaHuman
Convai
Google Gemini
OpenRouter
Python
FastAPI
```

---

## Building From Source

A complete source build requires the appropriate Unreal Engine project assets, plugins, and authorized external-service credentials.

General development requirements include:

1. Unreal Engine 5.7
2. Visual Studio with Unreal Engine C++ support
3. Required Unreal Engine plugins
4. Convai Unreal Engine integration
5. Authorized Gemini / OpenRouter credentials where applicable
6. Required external evaluation configuration where applicable

Private credentials are **not** included in the public repository.

---

# 🙏 Acknowledgements

Special thanks to:

- **Dr. Mohsin Sajjad** for his guidance and support throughout the Capstone project.
- **North South University**
- **Department of Electrical and Computer Engineering, North South University**
- **Epic Games** for Unreal Engine and MetaHuman.
- **Convai** for conversational AI technology.
- **Google** for the Gemini API.
- **OpenRouter** for multi-model AI access.
- Faculty members, reviewers, judges.

---

# 📄 License

InterviewSim is an academic Capstone Design Project developed by **Team G1** at **North South University**.

Copyright © 2026 Team G1

**All Rights Reserved.**

This repository is intended for:

- Educational use
- Academic research
- Project evaluation
- Demonstration
- Portfolio presentation

The source code and project materials may not be used commercially without authorization from the project contributors.

Third-party technologies, plugins, assets, APIs, trademarks, and services remain subject to their respective licenses and terms of use.

---

<p align="center">
  <b>InterviewSim Alpha 1.0</b>
</p>

<p align="center">
  <b>Practice. Perform. Improve.</b>
</p>

<p align="center">
  Developed by Team G1<br>
  North South University — 2026
</p>
