////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2023, Christoph Stiller. All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without 
// modification, are permitted provided that the following conditions are met:
// 
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation 
//    and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
////////////////////////////////////////////////////////////////////////////////

#include "zydec.h"

#include <string.h>

////////////////////////////////////////////////////////////////////////////////

bool zydec_WriteRaw(char **pBufferPos, size_t *pRemainingSize, const char *text);
bool zydec_WriteOperand(char **pBufferPos, size_t *pRemainingSize, const ZydisDecodedOperand *pOperand, const size_t virtualAddress);
bool zydec_WriteRegister(char **pBufferPos, size_t *pRemainingSize, const ZydisRegister reg);
bool zydec_WriteHex(char **pBufferPos, size_t *pRemainingSize, const uint64_t value);
bool zydec_WriteUInt(char **pBufferPos, size_t *pRemainingSize, const uint64_t value);
bool zydec_WriteInt(char **pBufferPos, size_t *pRemainingSize, const int64_t value);

////////////////////////////////////////////////////////////////////////////////

#define ERROR_CHECK(a) do { if (!(a)) return false; } while (false)

////////////////////////////////////////////////////////////////////////////////

bool zydec_TranslateInstructionWithoutContext(const ZydisDecodedInstruction *pInstruction, const ZydisDecodedOperand *pOperands, const size_t operandCount, const size_t virtualAddress, char *buffer, const size_t bufferCapacity, bool *pHasTranslation)
{
  if (pInstruction == nullptr || pOperands == nullptr || operandCount == 0 || buffer == nullptr || bufferCapacity == 0 || pHasTranslation == nullptr)
    return false;

  char *bufferPos = buffer;
  size_t remainingSize = bufferCapacity - 1;

  *pHasTranslation = true;
  bufferPos[0] = '\0';

  switch (pInstruction->mnemonic)
  {
  case ZYDIS_MNEMONIC_MOV:
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, " = "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[1], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_LEA:
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, " = &"));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[1], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_TEST:
  case ZYDIS_MNEMONIC_CMP:
  {
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "compare("));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ", "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[1], virtualAddress));

    if (pInstruction->mnemonic == ZYDIS_MNEMONIC_TEST)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ") // set carry_flag, parity_flag, zero_flag"));
    else if (pInstruction->mnemonic == ZYDIS_MNEMONIC_CMP)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ") // set carry_flag, overflow_flag, signed_flag, zero_flag, aux_carry_flag and parity_flag"));
    else
      return false;

    return true;
  }

  case ZYDIS_MNEMONIC_CALL:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "("));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ")()")); // TODO: Add callback for function names.
    break;

  case ZYDIS_MNEMONIC_JMP:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_JB:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (carry_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if below"));
    return true;

  case ZYDIS_MNEMONIC_JBE:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (carry_flag || zero_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if below or equal"));
    return true;

  case ZYDIS_MNEMONIC_JCXZ:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if ((u16)c == 0) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_JECXZ:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if ((u32)c == 0) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_JL:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (sign_flag != overflow_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if less"));
    return true;

  case ZYDIS_MNEMONIC_JLE:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (zero_flag || sign_flag != overflow_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if less or equal"));
    return true;

  case ZYDIS_MNEMONIC_JNB:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (!carry_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if not below"));
    return true;

  case ZYDIS_MNEMONIC_JNBE:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (!carry_flag && !zero_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if not below or equal"));
    return true;

  case ZYDIS_MNEMONIC_JNL:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (sign_flag && overflow_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if not less"));
    return true;

  case ZYDIS_MNEMONIC_JNLE:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (!zero_flag && sign_flag == overflow_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if not less or equal"));
    return true;

  case ZYDIS_MNEMONIC_JNO:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (!overflow_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_JNP:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (!parity_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_JNS:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (!sign_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_JNZ:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (!zero_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if not zero / not equal"));
    return true;

  case ZYDIS_MNEMONIC_JO:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (overflow_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_JP:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (parity_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_JS:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (sign_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_JZ:
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "if (zero_flag) goto "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "; // if zero / equal"));
    return true;

  case ZYDIS_MNEMONIC_SUB:
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, " -= "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[1], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_ADD:
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, " += "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[1], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_AND:
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, " &= "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[1], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_OR:
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, " |= "));
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[1], virtualAddress));
    break;

  case ZYDIS_MNEMONIC_MOVAPS:
  case ZYDIS_MNEMONIC_MOVAPD:
  case ZYDIS_MNEMONIC_VMOVDQA:
  case ZYDIS_MNEMONIC_VMOVDQA32:
  case ZYDIS_MNEMONIC_VMOVDQA64:
  {
    bool isReg2RegMove = false;

    if (pOperands[0].type == ZYDIS_OPERAND_TYPE_MEMORY || pOperands[0].type == ZYDIS_OPERAND_TYPE_POINTER)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_aligned_store"));
    else if (pOperands[1].type == ZYDIS_OPERAND_TYPE_MEMORY || pOperands[1].type == ZYDIS_OPERAND_TYPE_POINTER)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_aligned_load"));
    else
      isReg2RegMove = true;

    if (!isReg2RegMove)
    {
      switch (pInstruction->mnemonic)
      {
      case ZYDIS_MNEMONIC_MOVAPS:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_ps("));
        break;

      case ZYDIS_MNEMONIC_MOVAPD:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_pd("));
        break;

      case ZYDIS_MNEMONIC_VMOVDQA:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_si("));
        break;

      case ZYDIS_MNEMONIC_VMOVDQA32:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_epi32("));
        break;

      case ZYDIS_MNEMONIC_VMOVDQA64:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_epi64("));
        break;

      default:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "("));
        break;
      }
    }

    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));

    if (isReg2RegMove)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, " = "));
    else
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ", "));

    for (size_t operandIndex = 1; operandIndex < pInstruction->operand_count; operandIndex++)
    {
      if (operandIndex > 1)
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ", "));

      ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[operandIndex], virtualAddress));
    }

    if (!isReg2RegMove)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ")"));

    break;
  }

  case ZYDIS_MNEMONIC_MOVUPS:
  case ZYDIS_MNEMONIC_MOVUPD:
  case ZYDIS_MNEMONIC_MOVQ:
  case ZYDIS_MNEMONIC_LDDQU:
  case ZYDIS_MNEMONIC_VMOVD:
  case ZYDIS_MNEMONIC_VMOVDQU:
  case ZYDIS_MNEMONIC_VMOVDQU16:
  case ZYDIS_MNEMONIC_VMOVDQU32:
  case ZYDIS_MNEMONIC_VMOVDQU64:
  case ZYDIS_MNEMONIC_VMOVDQU8:
  {
    bool isReg2RegMove = false;

    if (pOperands[0].type == ZYDIS_OPERAND_TYPE_MEMORY || pOperands[0].type == ZYDIS_OPERAND_TYPE_POINTER)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_unaligned_store"));
    else if (pOperands[1].type == ZYDIS_OPERAND_TYPE_MEMORY || pOperands[1].type == ZYDIS_OPERAND_TYPE_POINTER)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_unaligned_load"));
    else
      isReg2RegMove = true;

    if (!isReg2RegMove)
    {
      switch (pInstruction->mnemonic)
      {
      case ZYDIS_MNEMONIC_MOVAPS:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_ps("));
        break;

      case ZYDIS_MNEMONIC_MOVAPD:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_pd("));
        break;

      case ZYDIS_MNEMONIC_MOVQ:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_si64("));
        break;

      case ZYDIS_MNEMONIC_LDDQU:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_cross_cache_line_si("));
        break;

      case ZYDIS_MNEMONIC_VMOVDQU:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_si("));
        break;

      case ZYDIS_MNEMONIC_VMOVDQU16:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_epi16("));
        break;

      case ZYDIS_MNEMONIC_VMOVDQU32:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_epi32("));
        break;

      case ZYDIS_MNEMONIC_VMOVDQU64:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_epi64("));
        break;

      case ZYDIS_MNEMONIC_VMOVDQU8:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_epi8("));
        break;

      default:
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "("));
        break;
      }
    }

    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));

    if (isReg2RegMove)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, " = "));
    else
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ", "));

    for (size_t operandIndex = 1; operandIndex < pInstruction->operand_count; operandIndex++)
    {
      if (operandIndex > 1)
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ", "));

      ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[operandIndex], virtualAddress));
    }

    if (!isReg2RegMove)
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ")"));

    break;
  }

  case ZYDIS_MNEMONIC_PAND:
  case ZYDIS_MNEMONIC_VPAND:
  case ZYDIS_MNEMONIC_VPANDQ:
  case ZYDIS_MNEMONIC_VPANDD:
  case ZYDIS_MNEMONIC_PANDN:
  case ZYDIS_MNEMONIC_VPANDN:
  case ZYDIS_MNEMONIC_VPANDNQ:
  case ZYDIS_MNEMONIC_VPANDND:
  case ZYDIS_MNEMONIC_PCMPEQB:
  case ZYDIS_MNEMONIC_PCMPEQW:
  case ZYDIS_MNEMONIC_PCMPEQD:
  case ZYDIS_MNEMONIC_PCMPEQQ:
  case ZYDIS_MNEMONIC_VPCMPEQB:
  case ZYDIS_MNEMONIC_VPCMPEQW:
  case ZYDIS_MNEMONIC_VPCMPEQD:
  case ZYDIS_MNEMONIC_VPCMPEQQ:
  case ZYDIS_MNEMONIC_PCMPGTB:
  case ZYDIS_MNEMONIC_PCMPGTW:
  case ZYDIS_MNEMONIC_PCMPGTD:
  case ZYDIS_MNEMONIC_PCMPGTQ:
  case ZYDIS_MNEMONIC_VPCMPGTB:
  case ZYDIS_MNEMONIC_VPCMPGTW:
  case ZYDIS_MNEMONIC_VPCMPGTD:
  case ZYDIS_MNEMONIC_VPCMPGTQ:
  case ZYDIS_MNEMONIC_PACKUSWB:
  case ZYDIS_MNEMONIC_PACKUSDW:
  case ZYDIS_MNEMONIC_VPACKUSWB:
  case ZYDIS_MNEMONIC_VPACKUSDW:
  case ZYDIS_MNEMONIC_PACKSSWB:
  case ZYDIS_MNEMONIC_PACKSSDW:
  case ZYDIS_MNEMONIC_VPACKSSWB:
  case ZYDIS_MNEMONIC_VPACKSSDW:
  case ZYDIS_MNEMONIC_PADDB:
  case ZYDIS_MNEMONIC_PADDW:
  case ZYDIS_MNEMONIC_PADDD:
  case ZYDIS_MNEMONIC_PADDQ:
  case ZYDIS_MNEMONIC_VPADDB:
  case ZYDIS_MNEMONIC_VPADDW:
  case ZYDIS_MNEMONIC_VPADDD:
  case ZYDIS_MNEMONIC_VPADDQ:
  case ZYDIS_MNEMONIC_PADDSB:
  case ZYDIS_MNEMONIC_PADDSW:
  case ZYDIS_MNEMONIC_VPADDSB:
  case ZYDIS_MNEMONIC_VPADDSW:
  case ZYDIS_MNEMONIC_EMMS:
  case ZYDIS_MNEMONIC_PMADDWD:
  case ZYDIS_MNEMONIC_VPMADDWD:
  case ZYDIS_MNEMONIC_PMULHW:
  case ZYDIS_MNEMONIC_VPMULHW:
  case ZYDIS_MNEMONIC_PMULLW:
  case ZYDIS_MNEMONIC_VPMULLW:
  case ZYDIS_MNEMONIC_POR:
  case ZYDIS_MNEMONIC_VPOR:
  case ZYDIS_MNEMONIC_VPORD:
  case ZYDIS_MNEMONIC_VPORQ:
  case ZYDIS_MNEMONIC_PABSW:
  case ZYDIS_MNEMONIC_VPABSW:
  case ZYDIS_MNEMONIC_PABSB:
  case ZYDIS_MNEMONIC_VPABSB:
  case ZYDIS_MNEMONIC_PABSD:
  case ZYDIS_MNEMONIC_VPABSD:
  case ZYDIS_MNEMONIC_ADDSUBPS:
  case ZYDIS_MNEMONIC_VADDSUBPS:
  case ZYDIS_MNEMONIC_ADDSUBPD:
  case ZYDIS_MNEMONIC_VADDSUBPD:
  case ZYDIS_MNEMONIC_PALIGNR:
  case ZYDIS_MNEMONIC_VPALIGNR:
  case ZYDIS_MNEMONIC_PAVGB:
  case ZYDIS_MNEMONIC_VPAVGB:
  case ZYDIS_MNEMONIC_PAVGW:
  case ZYDIS_MNEMONIC_VPAVGW:
  case ZYDIS_MNEMONIC_PBLENDW:
  case ZYDIS_MNEMONIC_VPBLENDW:
  case ZYDIS_MNEMONIC_PBLENDVB:
  case ZYDIS_MNEMONIC_VPBLENDVB:
  case ZYDIS_MNEMONIC_VPBLENDD:
  case ZYDIS_MNEMONIC_BLENDPS:
  case ZYDIS_MNEMONIC_VBLENDPS:
  case ZYDIS_MNEMONIC_BLENDPD:
  case ZYDIS_MNEMONIC_VBLENDPD:
  case ZYDIS_MNEMONIC_BLENDVPS:
  case ZYDIS_MNEMONIC_VBLENDVPS:
  case ZYDIS_MNEMONIC_BLENDVPD:
  case ZYDIS_MNEMONIC_VBLENDVPD:
  case ZYDIS_MNEMONIC_VBROADCASTF128:
  case ZYDIS_MNEMONIC_VBROADCASTF32X2:
  case ZYDIS_MNEMONIC_VBROADCASTF32X4:
  case ZYDIS_MNEMONIC_VBROADCASTF32X8:
  case ZYDIS_MNEMONIC_VBROADCASTF64X2:
  case ZYDIS_MNEMONIC_VBROADCASTF64X4:
  case ZYDIS_MNEMONIC_VBROADCASTI128:
  case ZYDIS_MNEMONIC_VBROADCASTI32X2:
  case ZYDIS_MNEMONIC_VBROADCASTI32X4:
  case ZYDIS_MNEMONIC_VBROADCASTI32X8:
  case ZYDIS_MNEMONIC_VBROADCASTI64X2:
  case ZYDIS_MNEMONIC_VBROADCASTI64X4:
  case ZYDIS_MNEMONIC_VBROADCASTSD:
  case ZYDIS_MNEMONIC_VBROADCASTSS:
  case ZYDIS_MNEMONIC_VPBROADCASTB:
  case ZYDIS_MNEMONIC_VPBROADCASTD:
  case ZYDIS_MNEMONIC_VPBROADCASTMB2Q:
  case ZYDIS_MNEMONIC_VPBROADCASTMW2D:
  case ZYDIS_MNEMONIC_VPBROADCASTQ:
  case ZYDIS_MNEMONIC_VPBROADCASTW:
  {
    ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[0], virtualAddress));
    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, " = "));

    switch (pInstruction->mnemonic)
    {
    case ZYDIS_MNEMONIC_PAND:
    case ZYDIS_MNEMONIC_VPAND:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_and_si("));
      break;

    case ZYDIS_MNEMONIC_VPANDQ:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_and_epi64("));
      break;

    case ZYDIS_MNEMONIC_VPANDD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_and_epi32("));
      break;

    case ZYDIS_MNEMONIC_PANDN:
    case ZYDIS_MNEMONIC_VPANDN:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_andnot_si("));
      break;

    case ZYDIS_MNEMONIC_VPANDNQ:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_andnot_epi64("));
      break;

    case ZYDIS_MNEMONIC_VPANDND:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_andnot_epi32("));
      break;

    case ZYDIS_MNEMONIC_PCMPEQB:
    case ZYDIS_MNEMONIC_VPCMPEQB:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_cmpeq_epi8("));
      break;

    case ZYDIS_MNEMONIC_PCMPEQW:
    case ZYDIS_MNEMONIC_VPCMPEQW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_cmpeq_epi16("));
      break;

    case ZYDIS_MNEMONIC_PCMPEQD:
    case ZYDIS_MNEMONIC_VPCMPEQD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_cmpeq_epi32("));
      break;

    case ZYDIS_MNEMONIC_PCMPEQQ:
    case ZYDIS_MNEMONIC_VPCMPEQQ:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_cmpeq_epi64("));
      break;

    case ZYDIS_MNEMONIC_PCMPGTB:
    case ZYDIS_MNEMONIC_VPCMPGTB:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_cmpgt_epi8("));
      break;

    case ZYDIS_MNEMONIC_PCMPGTW:
    case ZYDIS_MNEMONIC_VPCMPGTW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_cmpgt_epi16("));
      break;

    case ZYDIS_MNEMONIC_PCMPGTD:
    case ZYDIS_MNEMONIC_VPCMPGTD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_cmpgt_epi32("));
      break;

    case ZYDIS_MNEMONIC_PCMPGTQ:
    case ZYDIS_MNEMONIC_VPCMPGTQ:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_cmpgt_epi64("));
      break;

    case ZYDIS_MNEMONIC_PACKUSWB:
    case ZYDIS_MNEMONIC_VPACKUSWB:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_packus_epu16_to_epi8("));
      break;

    case ZYDIS_MNEMONIC_PACKUSDW:
    case ZYDIS_MNEMONIC_VPACKUSDW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_packus_epu32_to_epi16("));
      break;

    case ZYDIS_MNEMONIC_PACKSSWB:
    case ZYDIS_MNEMONIC_VPACKSSWB:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_packs_epu16_to_epi8("));
      break;

    case ZYDIS_MNEMONIC_PACKSSDW:
    case ZYDIS_MNEMONIC_VPACKSSDW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_packs_epu32_to_epi16("));
      break;

    case ZYDIS_MNEMONIC_PADDB:
    case ZYDIS_MNEMONIC_VPADDB:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_add_epi8("));
      break;

    case ZYDIS_MNEMONIC_PADDW:
    case ZYDIS_MNEMONIC_VPADDW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_add_epi16("));
      break;

    case ZYDIS_MNEMONIC_PADDD:
    case ZYDIS_MNEMONIC_VPADDD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_add_epi32("));
      break;

    case ZYDIS_MNEMONIC_PADDQ:
    case ZYDIS_MNEMONIC_VPADDQ:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_add_epi64("));
      break;

    case ZYDIS_MNEMONIC_PADDSB:
    case ZYDIS_MNEMONIC_PADDSW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_adds_epi8("));
      break;

    case ZYDIS_MNEMONIC_VPADDSB:
    case ZYDIS_MNEMONIC_VPADDSW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_adds_epi16("));
      break;

    case ZYDIS_MNEMONIC_EMMS:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_empty("));
      break;

    case ZYDIS_MNEMONIC_PMADDWD:
    case ZYDIS_MNEMONIC_VPMADDWD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_pmadd_epi16("));
      break;

    case ZYDIS_MNEMONIC_PMULHW:
    case ZYDIS_MNEMONIC_VPMULHW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_mulhi_epi16("));
      break;

    case ZYDIS_MNEMONIC_PMULLW:
    case ZYDIS_MNEMONIC_VPMULLW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_mullo_epi16("));
      break;

    case ZYDIS_MNEMONIC_POR:
    case ZYDIS_MNEMONIC_VPOR:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_or_si("));
      break;

    case ZYDIS_MNEMONIC_VPORD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_or_epi32("));
      break;

    case ZYDIS_MNEMONIC_VPORQ:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_or_epi64("));
      break;

    case ZYDIS_MNEMONIC_PABSB:
    case ZYDIS_MNEMONIC_VPABSB:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_abs_epi16("));
      break;

    case ZYDIS_MNEMONIC_PABSW:
    case ZYDIS_MNEMONIC_VPABSW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_abs_epi16("));
      break;

    case ZYDIS_MNEMONIC_PABSD:
    case ZYDIS_MNEMONIC_VPABSD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_abs_epi32("));
      break;

    case ZYDIS_MNEMONIC_ADDSUBPS:
    case ZYDIS_MNEMONIC_VADDSUBPS:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_addsub_ps("));
      break;
      
    case ZYDIS_MNEMONIC_ADDSUBPD:
    case ZYDIS_MNEMONIC_VADDSUBPD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_addsub_pd("));
      break;
      
    case ZYDIS_MNEMONIC_PALIGNR:
    case ZYDIS_MNEMONIC_VPALIGNR:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_alignr_epi8("));
      break;

    case ZYDIS_MNEMONIC_PAVGB:
    case ZYDIS_MNEMONIC_VPAVGB:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_avg_epu8("));
      break;

    case ZYDIS_MNEMONIC_PAVGW:
    case ZYDIS_MNEMONIC_VPAVGW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_avg_epu16("));
      break;

    case ZYDIS_MNEMONIC_PBLENDW:
    case ZYDIS_MNEMONIC_VPBLENDW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_blend_epi16("));
      break;

    case ZYDIS_MNEMONIC_VPBLENDD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_blend_epi32("));
      break;

    case ZYDIS_MNEMONIC_BLENDPS:
    case ZYDIS_MNEMONIC_VBLENDPS:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_blend_ps("));
      break;

    case ZYDIS_MNEMONIC_BLENDPD:
    case ZYDIS_MNEMONIC_VBLENDPD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_blend_pd("));
      break;

    case ZYDIS_MNEMONIC_PBLENDVB:
    case ZYDIS_MNEMONIC_VPBLENDVB:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_blendv_epi8("));
      break;

    case ZYDIS_MNEMONIC_BLENDVPS:
    case ZYDIS_MNEMONIC_VBLENDVPS:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_blendv_ps("));
      break;

    case ZYDIS_MNEMONIC_BLENDVPD:
    case ZYDIS_MNEMONIC_VBLENDVPD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_blendv_pd("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTF128:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_f128("));
      break;
      
    case ZYDIS_MNEMONIC_VBROADCASTF32X2:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_f32x2("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTF32X4:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_f32x4("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTF32X8:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_f32x8("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTF64X2:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_f64x2("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTF64X4:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_f64x4("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTI128:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcastsi128_si256("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTI32X2:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_i32x2("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTI32X4:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_i32x4("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTI32X8:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_i32x8("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTI64X2:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_i64x2("));
      break;

    case ZYDIS_MNEMONIC_VBROADCASTI64X4:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_i64x4("));
      break;
    
    case ZYDIS_MNEMONIC_VBROADCASTSD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_sd("));
      break;
      
    case ZYDIS_MNEMONIC_VBROADCASTSS:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_ss("));
      break;
      
    case ZYDIS_MNEMONIC_VPBROADCASTB:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_epi8("));
      break;

    case ZYDIS_MNEMONIC_VPBROADCASTW:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_epi16("));
      break;
      
    case ZYDIS_MNEMONIC_VPBROADCASTD:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_epi32("));
      break;

    case ZYDIS_MNEMONIC_VPBROADCASTQ:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcast_epi64("));
      break;
      
    case ZYDIS_MNEMONIC_VPBROADCASTMB2Q:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcastmb_epi64("));
      break;

    case ZYDIS_MNEMONIC_VPBROADCASTMW2D:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_broadcastmw_epi32("));
      break;

    default:
      ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, "_mm_??_("));
      break;
    }

    for (size_t operandIndex = 1; operandIndex < pInstruction->operand_count; operandIndex++)
    {
      if (operandIndex > 1)
        ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ", "));

      ERROR_CHECK(zydec_WriteOperand(&bufferPos, &remainingSize, &pOperands[operandIndex], virtualAddress));
    }

    ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ")"));

    break;
  }

  default:
    *pHasTranslation = false;
    return false;
  }

  ERROR_CHECK(zydec_WriteRaw(&bufferPos, &remainingSize, ";"));

  return true;
}

////////////////////////////////////////////////////////////////////////////////

static const char RegisterNameLut[][32] = {

    "",

    // General purpose registers  8-bit
    "(i8)a",
    "(i8)c",
    "(i8)d",
    "(i8)b",
    "(i8)(a >> 8)",
    "(i8)(c >> 8)",
    "(i8)(d >> 8)",
    "(i8)(b >> 8)",
    "(i8)stack_pointer",
    "(i8)bp",
    "(i8)si",
    "(i8)di",
    "(i8)r8",
    "(i8)r9",
    "(i8)r10",
    "(i8)r11",
    "(i8)r12",
    "(i8)r13",
    "(i8)r14",
    "(i8)r15",

    // General purpose registers 16-bit
    "(i16)a",
    "(i16)c",
    "(i16)d",
    "(i16)b",
    "(i16)stack_pointer",
    "(i16)bp",
    "(i16)si",
    "(i16)di",
    "(i16)r8",
    "(i16)r9",
    "(i16)r10",
    "(i16)r11",
    "(i16)r12",
    "(i16)r13",
    "(i16)r14",
    "(i16)r15",

    // General purpose registers 32-bit
    "(i32)ax",
    "(i32)cx",
    "(i32)dx",
    "(i32)bx",
    "(i32)stack_pointer",
    "(i32)bp",
    "(i32)si",
    "(i32)di",
    "(i32)r8",
    "(i32)r9",
    "(i32)r10",
    "(i32)r11",
    "(i32)r12",
    "(i32)r13",
    "(i32)r14",
    "(i32)r15",

    // General purpose registers 64-bit
    "(i64)a",
    "(i64)c",
    "(i64)d",
    "(i64)b",
    "(i64)stack_pointer",
    "(i64)bp",
    "(i64)si",
    "(i64)di",
    "(i64)r8",
    "(i64)r9",
    "(i64)r10",
    "(i64)r11",
    "(i64)r12",
    "(i64)r13",
    "(i64)r14",
    "(i64)r15",

    // Floating point legacy registers
    "(float)s0",
    "(float)s1",
    "(float)s2",
    "(float)s3",
    "(float)s4",
    "(float)s5",
    "(float)s6",
    "(float)s7",
    "x87control",
    "x87status",
    "x87tag",

    // Floating point multimedia registers
    "(float)mm0",
    "(float)mm1",
    "(float)mm2",
    "(float)mm3",
    "(float)mm4",
    "(float)mm5",
    "(float)mm6",
    "(float)mm7",

    // Floating point vector registers 128-bit
    "(m128)x0",
    "(m128)x1",
    "(m128)x2",
    "(m128)x3",
    "(m128)x4",
    "(m128)x5",
    "(m128)x6",
    "(m128)x7",
    "(m128)x8",
    "(m128)x9",
    "(m128)x10",
    "(m128)x11",
    "(m128)x12",
    "(m128)x13",
    "(m128)x14",
    "(m128)x15",
    "(m128)x16",
    "(m128)x17",
    "(m128)x18",
    "(m128)x19",
    "(m128)x20",
    "(m128)x21",
    "(m128)x22",
    "(m128)x23",
    "(m128)x24",
    "(m128)x25",
    "(m128)x26",
    "(m128)x27",
    "(m128)x28",
    "(m128)x29",
    "(m128)x30",
    "(m128)x31",

    // Floating point vector registers 256-bit
    "(m256)y0",
    "(m256)y1",
    "(m256)y2",
    "(m256)y3",
    "(m256)y4",
    "(m256)y5",
    "(m256)y6",
    "(m256)y7",
    "(m256)y8",
    "(m256)y9",
    "(m256)y10",
    "(m256)y11",
    "(m256)y12",
    "(m256)y13",
    "(m256)y14",
    "(m256)y15",
    "(m256)y16",
    "(m256)y17",
    "(m256)y18",
    "(m256)y19",
    "(m256)y20",
    "(m256)y21",
    "(m256)y22",
    "(m256)y23",
    "(m256)y24",
    "(m256)y25",
    "(m256)y26",
    "(m256)y27",
    "(m256)y28",
    "(m256)y29",
    "(m256)y30",
    "(m256)y31",

    // Floating point vector registers 512-bit
    "(m512)z0",
    "(m512)z1",
    "(m512)z2",
    "(m512)z3",
    "(m512)z4",
    "(m512)z5",
    "(m512)z6",
    "(m512)z7",
    "(m512)z8",
    "(m512)z9",
    "(m512)z10",
    "(m512)z11",
    "(m512)z12",
    "(m512)z13",
    "(m512)z14",
    "(m512)z15",
    "(m512)z16",
    "(m512)z17",
    "(m512)z18",
    "(m512)z19",
    "(m512)z20",
    "(m512)z21",
    "(m512)z22",
    "(m512)z23",
    "(m512)z24",
    "(m512)z25",
    "(m512)z26",
    "(m512)z27",
    "(m512)z28",
    "(m512)z29",
    "(m512)z30",
    "(m512)z31",

    // Matrix registers
    "(matrix_tile)t0",
    "(matrix_tile)t1",
    "(matrix_tile)t2",
    "(matrix_tile)t3",
    "(matrix_tile)t4",
    "(matrix_tile)t5",
    "(matrix_tile)t6",
    "(matrix_tile)t7",

    // Flags registers
    "flags",
    "eflags",
    "rflags",

    // Instruction-pointer registers
    "instruction_pointer",
    "instruction_pointer32",
    "instruction_pointer64",

    // Segment registers
    "extra_segment",
    "code_segment",
    "stack_segment",
    "data_segment",
    "f_segment",
    "g_segment",

    // Table registers
    "table_gdtr",
    "table_ldtr",
    "table_idtr",
    "table_tr",

    // Test registers
    "test_tr0",
    "test_tr1",
    "test_tr2",
    "test_tr3",
    "test_tr4",
    "test_tr5",
    "test_tr6",
    "test_tr7",

    // Control registers
    "control_cr0",
    "control_cr1",
    "control_cr2",
    "control_cr3",
    "control_cr4",
    "control_cr5",
    "control_cr6",
    "control_cr7",
    "control_cr8",
    "control_cr9",
    "control_cr10",
    "control_cr11",
    "control_cr12",
    "control_cr13",
    "control_cr14",
    "control_cr15",

    // Debug registers
    "debug_dr0",
    "debug_dr1",
    "debug_dr2",
    "debug_dr3",
    "debug_dr4",
    "debug_dr5",
    "debug_dr6",
    "debug_dr7",
    "debug_dr8",
    "debug_dr9",
    "debug_dr10",
    "debug_dr11",
    "debug_dr12",
    "debug_dr13",
    "debug_dr14",
    "debug_dr15",

    // Mask registers
    "mask_k0",
    "mask_k1",
    "mask_k2",
    "mask_k3",
    "mask_k4",
    "mask_k5",
    "mask_k6",
    "mask_k7",

    // Bound registers
    "bound_bnd0",
    "bound_bnd1",
    "bound_bnd2",
    "bound_bnd3",
    "bound_bndcfg",
    "bound_bndstatus",

    // Uncategorized
    "mxcsr",
    "pkru",
    "xcr0",
    "uif",
};

////////////////////////////////////////////////////////////////////////////////

bool zydec_WriteUInt(char **pBufferPos, size_t *pRemainingSize, const uint64_t value)
{
  char buffer[20];
  buffer[sizeof(buffer) - 1] = '\0';
  char *bufFromEnd = buffer + sizeof(buffer) - 2;
  uint64_t tmp = value;

  while (tmp >= 10)
  {
    *bufFromEnd = (char)('0' + (tmp % 10));
    bufFromEnd--;
    tmp /= 10;
  }

  if (tmp != 0)
    *bufFromEnd = (char)('0' + (tmp % 10));
  else
    bufFromEnd++;

  return zydec_WriteRaw(pBufferPos, pRemainingSize, bufFromEnd);
}

bool zydec_WriteHex(char **pBufferPos, size_t *pRemainingSize, const uint64_t value)
{
  char buffer[2 + 2 * 8 + 1];
  buffer[sizeof(buffer) - 1] = '\0';
  char *bufFromEnd = buffer + sizeof(buffer) - 2;
  uint64_t tmp = value;

  const char lut[] = "0123456789ABCDEF";

  while (tmp >= 0xF)
  {
    *bufFromEnd = lut[tmp & 0xF];
    bufFromEnd--;
    tmp >>= 4;
  }

  if (tmp != 0)
  {
    *bufFromEnd = lut[tmp & 0xF];
    bufFromEnd--;
  }

  *bufFromEnd = 'x';
  bufFromEnd--;
  *bufFromEnd = '0';

  return zydec_WriteRaw(pBufferPos, pRemainingSize, bufFromEnd);
}

bool zydec_WriteInt(char **pBufferPos, size_t *pRemainingSize, const int64_t value)
{
  if (value < 0)
  {
    ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, "-"));
    return zydec_WriteUInt(pBufferPos, pRemainingSize, (uint64_t)-value);
  }
  else
  {
    return zydec_WriteUInt(pBufferPos, pRemainingSize, (uint64_t)value);
  }
}

bool zydec_WriteOperand(char **pBufferPos, size_t *pRemainingSize, const ZydisDecodedOperand *pOperand, const size_t virtualAddress)
{
  switch (pOperand->type)
  {
  case ZYDIS_OPERAND_TYPE_REGISTER:
  {
    ERROR_CHECK(zydec_WriteRegister(pBufferPos, pRemainingSize, pOperand->reg.value));
    break;
  }

  case ZYDIS_OPERAND_TYPE_MEMORY:
  {
    ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, pOperand->mem.type == ZYDIS_MEMOP_TYPE_AGEN ? "(" : "*("));

    switch (pOperand->mem.type)
    {
    case ZYDIS_MEMOP_TYPE_MEM:
    {
      ERROR_CHECK(zydec_WriteRegister(pBufferPos, pRemainingSize, pOperand->mem.segment));
      ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, ": "));
      ERROR_CHECK(zydec_WriteRegister(pBufferPos, pRemainingSize, pOperand->mem.base));

      if (pOperand->mem.disp.has_displacement)
      {
        ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, " + "));
        ERROR_CHECK(zydec_WriteInt(pBufferPos, pRemainingSize, pOperand->mem.disp.value));
      }
      else if (pOperand->mem.index != ZYDIS_REGISTER_NONE)
      {
        ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, " + "));
        
        if (pOperand->mem.scale != 1)
          ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, "("));

        ERROR_CHECK(zydec_WriteRegister(pBufferPos, pRemainingSize, pOperand->mem.index));

        if (pOperand->mem.scale != 1)
        {
          ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, " * "));
          ERROR_CHECK(zydec_WriteUInt(pBufferPos, pRemainingSize, pOperand->mem.scale));
          ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, ")"));
        }
      }

      ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, ")"));

      break;
    }

    case ZYDIS_MEMOP_TYPE_MIB:
    case ZYDIS_MEMOP_TYPE_AGEN:
    {
      ERROR_CHECK(zydec_WriteRegister(pBufferPos, pRemainingSize, pOperand->mem.segment));
      ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, ": "));
      ERROR_CHECK(zydec_WriteRegister(pBufferPos, pRemainingSize, pOperand->mem.base));

      if (pOperand->mem.disp.has_displacement)
      {
        ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, " + "));
        ERROR_CHECK(zydec_WriteInt(pBufferPos, pRemainingSize, pOperand->mem.disp.value));
      }

      ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, ")"));

      break;
    }

    default:
      return false;
    }

    break;
  }

  case ZYDIS_OPERAND_TYPE_IMMEDIATE:
  {
    if (pOperand->imm.is_relative)
    {
      ERROR_CHECK(zydec_WriteHex(pBufferPos, pRemainingSize, virtualAddress + pOperand->imm.value.u));
    }
    else
    {
      if (pOperand->imm.is_signed)
        ERROR_CHECK(zydec_WriteInt(pBufferPos, pRemainingSize, pOperand->imm.value.s));
      else
        ERROR_CHECK(zydec_WriteUInt(pBufferPos, pRemainingSize, pOperand->imm.value.u));
    }

    break;
  }

  default:
    return false;
  }

  return true;
}

bool zydec_WriteRegister(char **pBufferPos, size_t *pRemainingSize, const ZydisRegister reg)
{
  if (reg >= sizeof(RegisterNameLut) / sizeof(RegisterNameLut[0]))
    return false;

  ERROR_CHECK(zydec_WriteRaw(pBufferPos, pRemainingSize, RegisterNameLut[reg]));

  return true;
}

bool zydec_WriteRaw(char **pBufferPos, size_t *pRemainingSize, const char *text)
{
  const size_t length = strlen(text);

  if (length > *pRemainingSize)
    return false;

  memcpy(*pBufferPos, text, length + 1);

  (*pRemainingSize) -= length;
  (*pBufferPos) += length;

  return true;
}
