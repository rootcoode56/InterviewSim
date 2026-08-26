# Poppler Runtime Dependency

InterviewSim uses **Poppler** for runtime text extraction from text-based PDF documents in the Real Interview document-processing pipeline.

Poppler itself is a third-party dependency and is **not included in this repository**.

## Required Location

Download a compatible Windows Poppler build and place the runtime files inside:

```text
ThirdParty/Poppler/bin/
```

The required executable used by InterviewSim is:

```text
ThirdParty/Poppler/bin/pdftotext.exe
```

The accompanying DLL files required by the selected Poppler build must remain in the same `bin` directory.

Expected structure:

```text
ThirdParty/
└── Poppler/
    └── bin/
        ├── pdftotext.exe
        ├── poppler.dll
        ├── freetype.dll
        ├── ...
        └── other required runtime DLLs
```

## InterviewSim Integration

The Poppler runtime is used by the InterviewSim Prompt Engine to extract text from supported text-based PDF documents.

The integration and project-relative runtime configuration were implemented as part of InterviewSim's document-processing system.

## Important

- Do not commit the Poppler `bin` directory to this repository.
- Poppler binaries and their dependencies remain subject to their respective licenses.
- Scanned or image-only PDFs may not contain extractable text and may not be supported by the current document-processing pipeline.

For Poppler project information, see:

https://poppler.freedesktop.org/