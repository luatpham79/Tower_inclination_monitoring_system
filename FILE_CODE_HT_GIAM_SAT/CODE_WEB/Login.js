const isNewUser = localStorage.getItem("password");

if (!isNewUser) {
  const registerPage = document.querySelector("#register");
  registerPage.style.display = "block";
} else {
  const loginPage = document.querySelector("#login");
  loginPage.style.display = "block";
}

const saltRounds = 10;

const hashPassword = async (password) => {
  return dcodeIO.bcrypt.hash(password, saltRounds);
};

const comparePassword = async (inputPassword, storedHash) => {
  return dcodeIO.bcrypt.compare(inputPassword, storedHash);
};

const handleRegister = async (event) => {
  event.preventDefault();
  const pass = document.querySelector("#regPass").value;
  const confirmPass = document.querySelector("#confirmPass").value;

  if (pass.length < 6) {
    alert("Mật khẩu quá ngắn!");
    return;
  }

  if (pass !== confirmPass) {
    alert("Mật khẩu xác nhận không khớp!");
    return;
  }

  try {
    const hashedPass = await hashPassword(pass);
    localStorage.setItem("password", hashedPass);
    alert("Đăng ký thành công!");
    location.reload();
  } catch (error) {
    console.error("Lỗi khi hash:", error);
  }
};

const handleLogin = async (event) => {
  event.preventDefault();
  const inputPass = document.querySelector("#loginPass").value;
  const storedHash = localStorage.getItem("password");

  if (!storedHash) {
    alert("Người dùng không tồn tại!");
    return;
  }

  const isMatch = await comparePassword(inputPass, storedHash);

  if (isMatch) {
    localStorage.setItem("isAuthenticated", true);
    window.location.href = "./Dashboard.html";
  } else {
    alert("Sai mật khẩu!");
  }
};
