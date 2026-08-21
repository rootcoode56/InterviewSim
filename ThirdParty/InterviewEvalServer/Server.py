from fastapi import FastAPI
from pydantic import BaseModel
import requests
import json
import os
import sys


app = FastAPI(title="Interview Evaluation Server")


def get_app_directory():
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)

    return os.path.dirname(os.path.abspath(__file__))


CONFIG_PATH = os.path.join(
    get_app_directory(),
    "evaluation_config.json"
)


def load_gemini_api_key():
    try:
        if not os.path.exists(CONFIG_PATH):
            return ""

        with open(CONFIG_PATH, "r", encoding="utf-8") as config_file:
            config_data = json.load(config_file)

        return str(config_data.get("gemini_api_key", "")).strip()

    except Exception:
        return ""


# Gemini key is loaded from external config file.
# It is NOT hardcoded inside Server.py or InterviewEvalServer.exe.
GEMINI_API_KEY = load_gemini_api_key()

GEMINI_URL = (
    "https://generativelanguage.googleapis.com/v1beta/"
    "models/gemini-3.6-flash:generateContent"
)


class EvaluationRequest(BaseModel):
    question: str = ""
    answer: str = ""


@app.get("/health")
def health():
    key_loaded = bool(GEMINI_API_KEY)

    return {
        "status": "ok",
        "service": "InterviewEvalServer",
        "gemini_key_loaded": key_loaded
    }


def clamp_score(value, default_value=0):
    try:
        number = float(value)

        if number < 0:
            return 0

        if number > 10:
            return 10

        return number

    except Exception:
        return default_value


def get_safe_public_error(error: Exception) -> str:
    if isinstance(error, requests.exceptions.Timeout):
        return "Gemini evaluation request timed out."

    if isinstance(error, requests.exceptions.ConnectionError):
        return "Could not connect to the Gemini evaluation service."

    if isinstance(error, requests.exceptions.HTTPError):
        response = getattr(error, "response", None)
        status_code = getattr(response, "status_code", None)

        if status_code is not None:
            return f"Gemini evaluation failed with HTTP status {status_code}."

        return "Gemini evaluation request failed."

    if isinstance(error, requests.exceptions.RequestException):
        return "Gemini evaluation request could not be completed."

    if isinstance(
        error,
        (
            json.JSONDecodeError,
            ValueError,
            KeyError,
            IndexError,
            TypeError,
            AttributeError
        )
    ):
        return "Gemini returned an invalid evaluation response."

    return "Gemini evaluation failed unexpectedly."


def count_pattern_occurrences(text: str, pattern: str) -> int:
    if not text or not pattern:
        return 0

    return text.count(pattern)


def count_grammar_mistakes_lightweight(answer: str) -> int:
    clean_answer = answer.strip()

    if not clean_answer:
        return 0

    mistake_count = 0

    lower_answer = clean_answer.lower()
    padded_answer = f" {lower_answer} "

    # Common contraction mistakes.
    # We do not count missing final punctuation because STT answers often do not include punctuation.
    mistake_count += count_pattern_occurrences(padded_answer, " im ")
    mistake_count += count_pattern_occurrences(padded_answer, " dont ")
    mistake_count += count_pattern_occurrences(padded_answer, " doesnt ")
    mistake_count += count_pattern_occurrences(padded_answer, " cant ")
    mistake_count += count_pattern_occurrences(padded_answer, " wont ")
    mistake_count += count_pattern_occurrences(padded_answer, " didnt ")
    mistake_count += count_pattern_occurrences(padded_answer, " isnt ")
    mistake_count += count_pattern_occurrences(padded_answer, " arent ")
    mistake_count += count_pattern_occurrences(padded_answer, " wasnt ")
    mistake_count += count_pattern_occurrences(padded_answer, " werent ")

    # Common subject-verb mistakes.
    mistake_count += count_pattern_occurrences(padded_answer, " i is ")
    mistake_count += count_pattern_occurrences(padded_answer, " i are ")
    mistake_count += count_pattern_occurrences(padded_answer, " he are ")
    mistake_count += count_pattern_occurrences(padded_answer, " she are ")
    mistake_count += count_pattern_occurrences(padded_answer, " it are ")
    mistake_count += count_pattern_occurrences(padded_answer, " we is ")
    mistake_count += count_pattern_occurrences(padded_answer, " they is ")
    mistake_count += count_pattern_occurrences(padded_answer, " you is ")

    # Common article mistakes.
    mistake_count += count_pattern_occurrences(padded_answer, " a actor ")
    mistake_count += count_pattern_occurrences(padded_answer, " a object ")
    mistake_count += count_pattern_occurrences(padded_answer, " a engine ")
    mistake_count += count_pattern_occurrences(padded_answer, " a example ")
    mistake_count += count_pattern_occurrences(padded_answer, " an class ")
    mistake_count += count_pattern_occurrences(padded_answer, " an pointer ")
    mistake_count += count_pattern_occurrences(padded_answer, " an function ")
    mistake_count += count_pattern_occurrences(padded_answer, " an variable ")

    return max(0, min(mistake_count, 10))


def local_fallback_evaluation(question: str, answer: str, error_message: str = ""):
    clean_answer = answer.strip()
    grammar_mistakes = count_grammar_mistakes_lightweight(clean_answer)

    if not clean_answer:
        return {
            "score": 0,
            "correctness": 0,
            "clarity": 0,
            "relevance": 0,
            "confidence": 0,
            "grammar_mistakes": 0,
            "repeated": False,
            "strengths": [],
            "weaknesses": ["No answer was provided."],
            "feedback": "No answer was detected. Please provide a complete response.",
            "source": "local_fallback",
            "error": error_message
        }

    length = len(clean_answer)

    if length < 20:
        score = 3
        feedback = "The answer is too short and needs more explanation."
        strengths = ["Candidate attempted to answer."]
        weaknesses = ["Answer lacks detail."]
    elif length < 80:
        score = 5
        feedback = "The answer is acceptable but needs more detail and examples."
        strengths = ["Candidate gave a relevant basic answer."]
        weaknesses = ["Answer could be more specific."]
    else:
        score = 7
        feedback = "The answer is detailed enough to continue the interview."
        strengths = ["Candidate provided a detailed response."]
        weaknesses = ["Answer can still be improved with stronger technical examples."]

    clarity = score

    if grammar_mistakes > 0:
        clarity = max(0, clarity - (grammar_mistakes * 0.5))
        feedback += " Grammar improvement is needed."
        weaknesses.append(f"Grammar mistakes detected: {grammar_mistakes}.")

    feedback += " This result was generated by local fallback evaluation."

    if error_message:
        feedback += " Gemini evaluation was unavailable."

    return {
        "score": score,
        "correctness": score,
        "clarity": clarity,
        "relevance": score,
        "confidence": score,
        "grammar_mistakes": grammar_mistakes,
        "repeated": False,
        "strengths": strengths,
        "weaknesses": weaknesses,
        "feedback": feedback,
        "source": "local_fallback",
        "error": error_message
    }


def extract_json_from_ai_text(ai_text: str):
    clean_text = ai_text.replace("```json", "")
    clean_text = clean_text.replace("```", "")
    clean_text = clean_text.strip()

    start_index = clean_text.find("{")
    end_index = clean_text.rfind("}")

    if start_index == -1 or end_index == -1 or end_index <= start_index:
        raise ValueError("No valid JSON object found in Gemini response.")

    json_text = clean_text[start_index:end_index + 1]

    return json.loads(json_text)


def normalize_evaluation_result(raw_result: dict, question: str, answer: str):
    fallback = local_fallback_evaluation(question, answer)

    grammar_mistakes = int(
        raw_result.get(
            "grammar_mistakes",
            fallback["grammar_mistakes"]
        )
    )

    strengths = raw_result.get("strengths", fallback["strengths"])
    weaknesses = raw_result.get("weaknesses", fallback["weaknesses"])

    if not isinstance(strengths, list):
        strengths = fallback["strengths"]

    if not isinstance(weaknesses, list):
        weaknesses = fallback["weaknesses"]

    if len(weaknesses) == 0:
        weaknesses = fallback["weaknesses"]

    return {
        "score": clamp_score(raw_result.get("score"), fallback["score"]),
        "correctness": clamp_score(raw_result.get("correctness"), fallback["correctness"]),
        "clarity": clamp_score(raw_result.get("clarity"), fallback["clarity"]),
        "relevance": clamp_score(raw_result.get("relevance"), fallback["relevance"]),
        "confidence": clamp_score(raw_result.get("confidence"), fallback["confidence"]),
        "grammar_mistakes": max(0, min(grammar_mistakes, 10)),
        "repeated": bool(raw_result.get("repeated", False)),
        "strengths": strengths,
        "weaknesses": weaknesses,
        "feedback": str(raw_result.get("feedback", fallback["feedback"])),
        "source": "gemini",
        "error": ""
    }

@app.post("/evaluate")
def evaluate(data: EvaluationRequest):
    question = data.question.strip()
    answer = data.answer.strip()

    key_loaded = bool(GEMINI_API_KEY)

    if not key_loaded:
        return local_fallback_evaluation(
            question,
            answer,
            "Gemini API key is missing or placeholder key is still used."
        )

    prompt = f"""
You are an expert technical interview evaluator.

Evaluate the candidate answer based on the interview question.

Question:
{question}

Candidate Answer:
{answer}

Return ONLY valid JSON in this exact format:

{{
  "score": 0,
  "correctness": 0,
  "clarity": 0,
  "relevance": 0,
  "confidence": 0,
  "grammar_mistakes": 0,
  "repeated": false,
  "strengths": ["..."],
  "weaknesses": ["..."],
  "feedback": "..."
}}

Rules:
- Score each numeric field from 0 to 10.
- grammar_mistakes must be a number from 0 to 10.
- repeated should be false unless the answer clearly repeats the same idea many times.
- Always include at least one strength.
- Always include at least one weakness or improvement suggestion.
- Be strict but fair.
- Do not return markdown.
- Do not return explanation outside JSON.
"""

    body = {
        "contents": [
            {
                "parts": [
                    {
                        "text": prompt
                    }
                ]
            }
        ]
    }

    try:
        response = requests.post(
            GEMINI_URL,
            headers={
                "Content-Type": "application/json",
                "x-goog-api-key": GEMINI_API_KEY
             },
             json=body,
             timeout=20
        )

        response.raise_for_status()

        result = response.json()

        ai_text = result["candidates"][0]["content"]["parts"][0]["text"]

        raw_result = extract_json_from_ai_text(ai_text)

        return normalize_evaluation_result(
            raw_result,
            question,
            answer
        )

    except Exception as error:
        safe_error = get_safe_public_error(error)

        return local_fallback_evaluation(
            question,
            answer,
            safe_error
        )