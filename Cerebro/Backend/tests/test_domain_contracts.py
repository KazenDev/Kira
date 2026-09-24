"""Contratos puros ejecutables también con pytest."""

from __future__ import annotations

import tempfile
from pathlib import Path

from app.domain.streaming import normalizar_emocion
from app.domain.tts import dividir_frases
from app.domain.vision import mensaje_con_foto, normalizar_foto
from app.hardware.audio_capture import consume_audio_capture
from app.hardware.ble_relay import RelayBroker
from app.intelligence.embeddings import HashEmbeddingProvider
from app.intelligence.rag_store import RagStore


def test_vision_preserves_multimodal_contract() -> None:
    photo = "data:image/jpeg;base64," + "A" * 120
    assert normalizar_foto(photo) == photo
    content = mensaje_con_foto("mira", photo)
    assert isinstance(content, list)
    assert content[1]["type"] == "image_url"


def test_streaming_and_tts_helpers() -> None:
    assert normalizar_emocion("confident") == "happy"
    assert dividir_frases("Hola. Esto es una frase suficiente.")[0]


def test_audio_parser_keeps_partial_markers() -> None:
    phase, buffer, audio, complete = consume_audio_capture(
        "esperando_start", b"", b"AUDIO:STAR"
    )
    assert not audio and not complete
    phase, buffer, audio, complete = consume_audio_capture(phase, buffer, b"T\nabc")
    assert phase == "audio" and audio == b"abc"
    phase, buffer, audio, complete = consume_audio_capture(
        "esperando_start", b"", b"AUDIO:CANCEL\n"
    )
    assert phase == "cancelado" and audio == b"" and complete
    phase, buffer, audio, complete = consume_audio_capture(
        "audio", b"", b"xyzAUDIO:CANCEL\n"
    )
    assert phase == "cancelado" and audio == b"" and complete


def test_relay_broker_does_not_duplicate_nack() -> None:
    broker = RelayBroker()
    broker.register("owner")
    broker.enqueue("TALK\n")
    broker.return_lines(["TALK"])
    assert broker.take() == ["TALK\n"]
    assert broker.take() == []


def test_rag_store_keeps_deterministic_vector_path() -> None:
    with tempfile.TemporaryDirectory(prefix="kira_pytest_rag_") as directory:
        store = RagStore(Path(directory) / "index.sqlite3", HashEmbeddingProvider())
        records = [{"id": "a", "texto": "Mateo prefiere las tomboys"}]
        store.sync_memories("kira", records, embed=True)
        assert store.search("kira", "tomboys", candidates=records)[0]["id"] == "a"
        assert store.stats()["vectors"] == 1
