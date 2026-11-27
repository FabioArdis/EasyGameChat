import inspect
import json
import threading

import pytest

from easygamechat import (
    EasyGameChat,
    EasyGameChatError,
    TLSError,
    egc_connect,
    egc_create,
    egc_create_with_certs,
    egc_destroy,
)
from easygamechat.easygamechat import MAX_MESSAGE_LENGTH, _split_first_json


def test_tls_error_is_exported():
    assert issubclass(TLSError, EasyGameChatError)


def test_invalid_ca_certificate_raises_tls_error(tmp_path):
    with pytest.raises(TLSError):
        EasyGameChat("127.0.0.1", 3000, ca_cert_path=str(tmp_path / "missing.pem"))


def test_egc_create_with_certs_is_exported():
    assert callable(egc_create_with_certs)


def test_connect_token_defaults_to_empty():
    signature = inspect.signature(EasyGameChat.connect)
    assert signature.parameters["token"].default == ""


def test_egc_connect_passes_token(monkeypatch):
    handle = egc_create("127.0.0.1", 3000, use_tls=False)
    assert handle is not None

    calls = {}

    def fake_connect(self, nickname, token=""):
        calls["args"] = (nickname, token)
        return True

    monkeypatch.setattr(EasyGameChat, "connect", fake_connect)
    try:
        assert egc_connect(handle, "alice", "secret")
        assert calls["args"] == ("alice", "secret")
    finally:
        egc_destroy(handle)


def test_split_first_json_with_coalesced_messages():
    first, rest = _split_first_json(
        b'{"status":"success"}{"from":"Server","text":"alice joined"}\n'
    )
    assert json.loads(first) == {"status": "success"}
    assert rest == b'{"from":"Server","text":"alice joined"}\n'


def test_split_first_json_ignores_braces_in_strings():
    first, rest = _split_first_json(b'{"text":"}"}{"text":"next"}')
    assert json.loads(first)["text"] == "}"
    assert rest == b'{"text":"next"}'


def test_split_first_json_keeps_incomplete_object():
    message, rest = _split_first_json(b'{"text":"hel')
    assert message is None
    assert rest == b'{"text":"hel'


def test_process_message_accepts_max_length_text():
    client = EasyGameChat("127.0.0.1", 3000, use_tls=False)
    received = []
    client.set_message_callback(lambda f, t: received.append((f, t)))

    text = "a" * MAX_MESSAGE_LENGTH
    client._process_message(json.dumps({"from": "alice", "text": text}))

    assert received == [("alice", text)]


def test_callback_can_clear_itself():
    client = EasyGameChat("127.0.0.1", 3000, use_tls=False)
    called = []

    def callback(from_user, text):
        called.append((from_user, text))
        client.set_message_callback(None)

    client.set_message_callback(callback)
    thread = threading.Thread(
        target=client._process_message,
        args=(json.dumps({"from": "alice", "text": "hello"}),),
        daemon=True,
    )
    thread.start()
    thread.join(timeout=1.0)

    assert not thread.is_alive()
    assert called == [("alice", "hello")]
