from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from database import engine
import models
from routers import auth, classes, presentations, scores

models.Base.metadata.create_all(bind=engine)

app = FastAPI(title="학생 발표 평가 시스템", version="1.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(auth.router, prefix="/api/auth", tags=["인증"])
app.include_router(classes.router, prefix="/api/classes", tags=["반 관리"])
app.include_router(presentations.router, prefix="/api/presentations", tags=["발표"])
app.include_router(scores.router, prefix="/api/scores", tags=["평가"])

@app.get("/api/health")
def health_check():
    return {"status": "ok"}
