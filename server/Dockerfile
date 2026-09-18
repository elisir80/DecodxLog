# DecoLog Cloud: un'immagine sola, senza fronzoli.
FROM python:3.12-slim

ENV PYTHONDONTWRITEBYTECODE=1 PYTHONUNBUFFERED=1
WORKDIR /app

COPY requirements.txt .
RUN pip install --no-cache-dir -r requirements.txt

COPY decolog_cloud ./decolog_cloud

# Il servizio non gira da root.
RUN useradd --create-home decolog && chown -R decolog /app
USER decolog

EXPOSE 8000
CMD ["uvicorn", "decolog_cloud.main:app", "--host", "0.0.0.0", "--port", "8000"]
